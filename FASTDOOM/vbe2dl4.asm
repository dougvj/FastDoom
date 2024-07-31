;
; DESCRIPTION: Assembly texture mapping routines for VESA modes
;

BITS 32
%include "macros.inc"

%ifdef MODE_VBE2_DIRECT
%include "defs.inc"

extern _destview

;============================================================================
; unwound vertical scaling code
;
; eax   light table pointer, 0 lowbyte overwritten
; ebx   all 0, low byte overwritten
; ecx   fractional step value
; edx   fractional scale value
; esi   start of source pixels
; edi   bottom pixel in screenbuffer to blit into
;
; ebx should be set to 0 0 0 dh to feed the pipeline
;
; The graphics wrap vertically at 128 pixels
;============================================================================

BEGIN_DATA_SECTION

%macro SCALEDEFINE 1
  dd vscale%1
%endmacro

align 4

scalecalls:
  %assign LINE 0
  %rep SCREENHEIGHT+1
    SCALEDEFINE LINE
  %assign LINE LINE+1
  %endrep

;============================================================================

BEGIN_CODE_SECTION

;============================================================================
;
; R_DrawColumnLowVBE2
;
;============================================================================

CODE_SYM_DEF R_DrawColumnLowVBE2Flat
	push		edi
	push		ebp
  push		ebx

  mov  ebp,[_dc_yh]
  MulScreenWidthStart edi, ebp
  sub  ebp,[_dc_yl] ; ebp = pixel count
  js   near .done

  MulScreenWidthEnd edi
  mov  ebx,[_dc_x]
  mov  al,[_dc_color]
  lea  edi,[edi+ebx*2]
  mov  ah,al
  add  edi,[_destview]

  jmp  [scalecalls+4+ebp*4]

.done:
	pop		ebx
  pop	  ebp
	pop		edi
  ret
; R_DrawColumnLowVBE2 ends

;============ HIGH DETAIL ============

%macro SCALELABEL 1
  vscale%1
%endmacro

%assign LINE SCREENHEIGHT
%rep SCREENHEIGHT-1
  SCALELABEL LINE:
    mov  [edi-(LINE-1)*SCREENWIDTH],ax  ; draw a pixel to the buffer
    %assign LINE LINE-1
%endrep

vscale1:
  pop	ebx
  pop	ebp
  mov [edi],ax

vscale0:
	pop		edi
  ret

%endif
