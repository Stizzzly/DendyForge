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
    ; Zero page, X
    LDA #$42
    STA $15
    LDX #$05
    LDA $10,X
    STA $0200

    ; Zero page, Y
    LDX #$77
    STX $25
    LDY #$05
    LDX $20,Y
    STX $0201

    ; Absolute
    LDA #$A1
    STA $0300
    LDA $0300
    STA $0202

    ; Absolute, X and Absolute, Y, both crossing a page
    LDA #$B2
    STA $0300
    LDX #$01
    LDA $02FF,X
    STA $0203

    LDA #$C3
    STA $0300
    LDY #$01
    LDA $02FF,Y
    STA $0204

    ; Indexed indirect: ($1C + X) -> $0300
    LDA #$00
    STA $20
    LDA #$03
    STA $21
    LDA #$D4
    STA $0300
    LDX #$04
    LDA ($1C,X)
    STA $0205

    ; Indirect indexed: ($30) + Y -> $0300
    LDA #$FF
    STA $30
    LDA #$02
    STA $31
    LDA #$E5
    STA $0300
    LDY #$01
    LDA ($30),Y
    STA $0206

    ; Relative addressing: BNE is not taken, BEQ is taken.
    LDA #$00
    BNE Failure
    BEQ BranchTarget

Failure:
    LDA #$00
    STA $0207
    JMP Forever

BranchTarget:
    LDA #$F6
    STA $0207

    ; JMP ($00FF) takes its high byte from $0000 on CPU6502.
    LDA #<IndirectTarget
    STA $FF
    LDA #>IndirectTarget
    STA $00
    LDA #$81
    STA $0100
    .byte $6C, $FF, $00 ; JMP ($00FF)

IndirectTarget:
    LDA #$A7
    STA $0208

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
