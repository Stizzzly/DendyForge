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
    ; 45 + 55 is 00 with carry in BCD and 9A in binary.
    SED
    CLC
    LDA #$45
    ADC #$55
    STA $00

    ; 50 - 01 is 49 in BCD and 4F in binary.
    SEC
    LDA #$50
    SBC #$01
    STA $01

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
