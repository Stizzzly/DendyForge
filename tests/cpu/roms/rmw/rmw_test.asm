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
    CLC
    LDA #$80
    STA $10
    ROL $10

    SEC
    LDA #$01
    STA $11
    ROR $11

    LDA #$02
    STA $12
    LSR $12

    LDA #$FF
    STA $13
    INC $13

    LDA #$00
    STA $14
    DEC $14

    LDX #$01
    LDA #$FF
    STA $21
    INC $20,X
    LDA #$00
    STA $22
    DEC $21,X

    CLC
    LDA #$80
    STA $0200
    ROL $0200

    SEC
    LDA #$01
    STA $0201
    ROR $0201

    LDA #$02
    STA $0202
    LSR $0202

    LDA #$FF
    STA $0203
    INC $0203

    LDA #$00
    STA $0204
    DEC $0204

    LDX #$01
    LDA #$FF
    STA $0206
    INC $0205,X
    LDA #$00
    STA $0207
    DEC $0206,X

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
