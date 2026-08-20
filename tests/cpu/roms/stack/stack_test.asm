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
    PHA
    LDA #$00
    PLA
    STA $0200

    CLC
    CLD
    PHP
    SEC
    SED
    PLP
    BCC CarryRestored

    LDA #$00
    STA $0201
    JMP Forever

CarryRestored:
    LDA #$01
    STA $0201

    JSR SetResult
    STA $0202

Forever:
    JMP Forever

SetResult:
    LDA #$55
    RTS

Dummy_Int:
    RTI

.segment "VECTORS"

.word Dummy_Int
.word Reset
.word Dummy_Int

.segment "CHARS"

.res 8192
