.segment "HEADER"

.byte 'N','E','S',$1A
.byte 2
.byte 1
.byte $00
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

    LDA #$42
    TAX

    LDA #$11
    TAY

    LDX #$77
    TXA

    LDY #$88
    TYA

    LDX #$55
    TXS

    TSX

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
