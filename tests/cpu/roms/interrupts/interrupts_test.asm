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
    BRK
    .byte $EA

    LDA #$55
    STA $0201

Forever:
    JMP Forever

BrkHandler:
    LDA #$42
    STA $0200
    RTI

NmiHandler:
    RTI

.segment "VECTORS"

.word NmiHandler
.word Reset
.word BrkHandler

.segment "CHARS"

.res 8192
