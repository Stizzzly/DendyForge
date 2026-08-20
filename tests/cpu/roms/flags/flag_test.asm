.segment "HEADER"

.byte 'N','E','S',$1A
.byte 2         ; 32KB PRG ROM
.byte 1         ; 8KB CHR ROM
.byte $00       ; Mapper 0
.byte $00
.byte $00,$00,$00,$00,$00,$00,$00,$00


.segment "STARTUP"

Reset:
    SEI
    CLD
    LDX #$FF
    TXS

    JMP Main


.segment "CODE"

Main:

; -------------------------------------------------------
; Flag Instructions
; -------------------------------------------------------

    SEC
    CLC

    SEI
    CLI

    SED
    CLD

    CLV


Forever:
    JMP Forever


Dummy_Int:
    RTI


.segment "VECTORS"

.word Dummy_Int
.word Reset
.word Dummy_Int


.segment "CHARS"

.res 8192
