#include <stdlib.h>
#include <dos.h>
#include <conio.h>
#include "ns_dpmi.h"
#include "ns_task.h"
#include "ns_cards.h"
#include "ns_user.h"
#include "ns_lpt.h"
#include "ns_muldf.h"

#include "m_misc.h"
#include "options.h"
#include "doomstat.h"

static int LPT_Installed = 0;

static char *LPT_BufferStart;
static char *LPT_CurrentBuffer;
static int LPT_BufferNum = 0;
static int LPT_NumBuffers = 0;
static int LPT_TransferLength = 0;
static int LPT_CurrentLength = 0;

static char *LPT_SoundPtr;
volatile int LPT_SoundPlaying;

static int LPT_Port = 0x378;

static task *LPT_Timer;

void (*LPT_CallBack)(void);

/*---------------------------------------------------------------------
   Function: LPT_ServiceInterrupt

   Handles interrupt generated by sound card at the end of a voice
   transfer.  Calls the user supplied callback function.
---------------------------------------------------------------------*/

static void LPT_ServiceInterrupt(task *Task)
{
    outp(LPT_Port, *LPT_SoundPtr);

    LPT_SoundPtr++;

    LPT_CurrentLength--;
    if (LPT_CurrentLength == 0)
    {
        // Keep track of current buffer
        LPT_CurrentBuffer += LPT_TransferLength;
        LPT_BufferNum++;
        if (LPT_BufferNum >= LPT_NumBuffers)
        {
            LPT_BufferNum = 0;
            LPT_CurrentBuffer = LPT_BufferStart;
        }

        LPT_CurrentLength = LPT_TransferLength;
        LPT_SoundPtr = LPT_CurrentBuffer;

        // Call the caller's callback function
        if (LPT_CallBack != NULL)
        {
            MV_ServiceVoc();
        }
    }
}

/*---------------------------------------------------------------------
   Function: LPT_StopPlayback

   Ends the transfer of digitized sound to the Sound Source.
---------------------------------------------------------------------*/

void LPT_StopPlayback(void)
{
    if (LPT_SoundPlaying)
    {
        TS_Terminate(LPT_Timer);
        LPT_SoundPlaying = 0;
        LPT_BufferStart = NULL;
    }
}

/*---------------------------------------------------------------------
   Function: LPT_BeginBufferedPlayback

   Begins multibuffered playback of digitized sound on the Sound Source.
---------------------------------------------------------------------*/

int LPT_BeginBufferedPlayback(
    char *BufferStart,
    int BufferSize,
    int NumDivisions,
    void (*CallBackFunc)(void))
{
    if (LPT_SoundPlaying)
    {
        LPT_StopPlayback();
    }

    LPT_CallBack = CallBackFunc;

    LPT_BufferStart = BufferStart;
    LPT_CurrentBuffer = BufferStart;
    LPT_SoundPtr = BufferStart;
    // VITI95: OPTIMIZE
    LPT_TransferLength = BufferSize / NumDivisions;
    LPT_CurrentLength = LPT_TransferLength;
    LPT_BufferNum = 0;
    LPT_NumBuffers = NumDivisions;

    LPT_SoundPlaying = 1;

    LPT_Timer = TS_ScheduleTask(LPT_ServiceInterrupt, LPT_SampleRate, 1, NULL);
    TS_Dispatch();

    return (LPT_Ok);
}

/*---------------------------------------------------------------------
   Function: LPT_Init

   Initializes the Sound Source prepares the module to play digitized
   sounds.
---------------------------------------------------------------------*/

int LPT_Init(int soundcard, int port)
{
    if (LPT_Installed)
    {
        LPT_Shutdown();
    }

    LPT_SoundPlaying = 0;

    LPT_CallBack = NULL;

    LPT_BufferStart = NULL;

    if (port != -1)
        LPT_Port = port;

    LPT_Installed = 1;

    return (LPT_Ok);
}

/*---------------------------------------------------------------------
   Function: LPT_Shutdown

   Ends transfer of sound data to the Sound Source.
---------------------------------------------------------------------*/

void LPT_Shutdown(void)
{
    LPT_StopPlayback();

    LPT_SoundPlaying = 0;

    LPT_BufferStart = NULL;

    LPT_CallBack = NULL;

    LPT_Installed = 0;
}
