;
; DESCRIPTION: Assembly texture mapping routines for VESA modes
;

BITS 32
%include "macros.inc"

%ifdef MODE_VBE2_DIRECT
%include "defs.inc"

extern _destview
extern _centery

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

CODE_SYM_DEF R_DrawColumnLowVBE2Direct
	push		edi
	push		esi
	push		edx
	push		ebp
  push		ebx
	push		ecx

  mov  ebp,[_dc_yh]
  mov  eax,[_dc_yl]
  MulScreenWidthStart edi, ebp
  sub  ebp,eax ; ebp = pixel count
  js   near donel

  mov  ebx,[_dc_x]
  MulScreenWidthEnd edi
  mov   esi,[_dc_source]
  lea  edi,[edi+ebx*2]
  mov   eax,[_dc_colormap]
  add  edi,[_destview]

  jmp  [scalecalls+4+ebp*4]

donel:
	pop		ecx
	pop		ebx
  pop	  ebp
	pop		edx
	pop		esi
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
    mov  al,[esi]                   ; get source pixel
    mov  bl,[eax]                       ; translate the color
    mov  bh,bl
    inc  esi
    mov  [edi-(LINE-1)*SCREENWIDTH],bx  ; draw a pixel to the buffer
    %assign LINE LINE-1
%endrep

vscale1:
  pop	ecx
  pop	ebx
  mov al,[esi]
  pop	ebp
  mov al,[eax]
  pop	edx
  mov ah, al
  pop	esi
  mov [edi],ax

vscale0:
	pop		edi
  ret

%endif
