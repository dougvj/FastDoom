#include <stdlib.h>
#include <dos.h>
#include <conio.h>
#include "ns_dpmi.h"
#include "ns_task.h"
#include "ns_cards.h"
#include "ns_user.h"
#include "ns_adbfx.h"
#include "ns_sb.h"
#include "ns_sbdef.h"
#include "ns_sbmus.h"
#include "ns_muldf.h"
#include "ns_fxm.h"

#include "m_misc.h"

#include "doomstat.h"

#include "options.h"

static int ADBFX_Installed = 0;

static char *ADBFX_BufferStart;
static char *ADBFX_CurrentBuffer;
static int ADBFX_BufferNum = 0;
static int ADBFX_NumBuffers = 0;
static int ADBFX_TransferLength = 0;
static int ADBFX_CurrentLength = 0;

static char *ADBFX_SoundPtr;
volatile int ADBFX_SoundPlaying;

static task *ADBFX_Timer;

void (*ADBFX_CallBack)(void);

/*---------------------------------------------------------------------
   Function: ADBFX_ServiceInterrupt

   Handles interrupt generated by sound card at the end of a voice
   transfer.  Calls the user supplied callback function.
---------------------------------------------------------------------*/

const unsigned char AdlibLUTdb[256] = {
    63, 63, 56, 51, 48, 46, 43, 42,
    40, 39, 38, 36, 35, 34, 34, 33,
    32, 31, 31, 30, 29, 29, 28, 28,
    27, 27, 26, 26, 26, 25, 25, 24,
    24, 24, 23, 23, 23, 22, 22, 22,
    21, 21, 21, 21, 20, 20, 20, 20,
    19, 19, 19, 19, 18, 18, 18, 18,
    18, 17, 17, 17, 17, 17, 16, 16,
    16, 16, 16, 15, 15, 15, 15, 15,
    15, 14, 14, 14, 14, 14, 14, 14,
    13, 13, 13, 13, 13, 13, 13, 12,
    12, 12, 12, 12, 12, 12, 12, 11,
    11, 11, 11, 11, 11, 11, 11, 10,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0};

static void ADBFX_ServiceInterrupt(task *Task)
{
    unsigned char value = AdlibLUTdb[(unsigned char)(*ADBFX_SoundPtr)];

    outp(ADLIB_PORT + 1, value);

    ADBFX_SoundPtr++;

    ADBFX_CurrentLength--;
    if (ADBFX_CurrentLength == 0)
    {
        // Keep track of current buffer
        ADBFX_CurrentBuffer += ADBFX_TransferLength;
        ADBFX_BufferNum++;
        if (ADBFX_BufferNum >= ADBFX_NumBuffers)
        {
            ADBFX_BufferNum = 0;
            ADBFX_CurrentBuffer = ADBFX_BufferStart;
        }

        ADBFX_CurrentLength = ADBFX_TransferLength;
        ADBFX_SoundPtr = ADBFX_CurrentBuffer;

        // Call the caller's callback function
        if (ADBFX_CallBack != NULL)
        {
            MV_ServiceVoc();
        }
    }
}

/*---------------------------------------------------------------------
   Function: ADBFX_StopPlayback

   Ends the transfer of digitized sound to the Sound Source.
---------------------------------------------------------------------*/

void ADBFX_StopPlayback(void)
{
    if (ADBFX_SoundPlaying)
    {
        TS_Terminate(ADBFX_Timer);
        ADBFX_SoundPlaying = 0;
        ADBFX_BufferStart = NULL;

        AL_Reset();
    }
}

/*---------------------------------------------------------------------
   Function: ADBFX_BeginBufferedPlayback

   Begins multibuffered playback of digitized sound on the Sound Source.
---------------------------------------------------------------------*/

int ADBFX_BeginBufferedPlayback(
    char *BufferStart,
    int BufferSize,
    int NumDivisions,
    void (*CallBackFunc)(void))
{
    if (ADBFX_SoundPlaying)
    {
        ADBFX_StopPlayback();
    }

    ADBFX_CallBack = CallBackFunc;

    ADBFX_BufferStart = BufferStart;
    ADBFX_CurrentBuffer = BufferStart;
    ADBFX_SoundPtr = BufferStart;
    // VITI95: OPTIMIZE
    ADBFX_TransferLength = BufferSize / NumDivisions;
    ADBFX_CurrentLength = ADBFX_TransferLength;
    ADBFX_BufferNum = 0;
    ADBFX_NumBuffers = NumDivisions;

    ADBFX_SoundPlaying = 1;

    ADBFX_Timer = TS_ScheduleTask(ADBFX_ServiceInterrupt, FX_MixRate, 1, NULL);
    TS_Dispatch();

    return (ADBFX_Ok);
}

/*---------------------------------------------------------------------
   Function: ADBFX_Init

   Initializes the Sound Source prepares the module to play digitized
   sounds.
---------------------------------------------------------------------*/

int ADBFX_Init(int soundcard)
{
    int i;

    if (ADBFX_Installed)
    {
        ADBFX_Shutdown();
    }

    AL_Reset();

    AL_SendOutputToPort(ADLIB_PORT, 0x20, 0x21);
    AL_SendOutputToPort(ADLIB_PORT, 0x60, 0xF0);
    AL_SendOutputToPort(ADLIB_PORT, 0x80, 0xF0);
    AL_SendOutputToPort(ADLIB_PORT, 0xC0, 0x01);
    AL_SendOutputToPort(ADLIB_PORT, 0xE0, 0x00);
    AL_SendOutputToPort(ADLIB_PORT, 0x43, 0x3F);
    AL_SendOutputToPort(ADLIB_PORT, 0xB0, 0x01);
    AL_SendOutputToPort(ADLIB_PORT, 0xA0, 0x8F);
    AL_SendOutputToPort(ADLIB_PORT, 0xB0, 0x2E);

    /* Wait */
    for (i = 0; i < 500; i++)
    {
        inp(ADLIB_PORT);
    }

    AL_SendOutputToPort(ADLIB_PORT, 0xB0, 0x20);
    AL_SendOutputToPort(ADLIB_PORT, 0xA0, 0x00);

    /* First sample */
    AL_SendOutputToPort(ADLIB_PORT, 0x40, 0x00);

    ADBFX_SoundPlaying = 0;

    ADBFX_CallBack = NULL;

    ADBFX_BufferStart = NULL;

    ADBFX_Installed = 1;

    return (ADBFX_Ok);
}

/*---------------------------------------------------------------------
   Function: ADBFX_Shutdown

   Ends transfer of sound data to the Sound Source.
---------------------------------------------------------------------*/

void ADBFX_Shutdown(void)
{
    ADBFX_StopPlayback();

    ADBFX_SoundPlaying = 0;

    ADBFX_BufferStart = NULL;

    ADBFX_CallBack = NULL;

    ADBFX_Installed = 0;
}
