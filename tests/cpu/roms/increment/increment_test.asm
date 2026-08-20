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
; Increment / Decrement
; -------------------------------------------------------

    LDX #$10
    INX          ; -> $11

    LDY #$20
    INY          ; -> $21

    LDX #$30
    DEX          ; -> $2F

    LDY #$40
    DEY          ; -> $3F

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
