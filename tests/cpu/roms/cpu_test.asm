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
; ABS
; -------------------------------------------------------

    LDA #$42
    STA $0200

    LDX #$11
    STX $0201

    LDY #$22
    STY $0202


; -------------------------------------------------------
; Zero Page
; -------------------------------------------------------

    LDA #$AA
    STA $00

    LDX #$BB
    STX $01

    LDY #$CC
    STY $02


; -------------------------------------------------------
; Zero Page,X / Zero Page,Y
; -------------------------------------------------------

    LDX #$05
    LDA #$42
    STA $10,X          ; -> $0015

    LDY #$03
    LDX #$99
    STX $20,Y          ; -> $0023

    LDX #$07
    LDY #$55
    STY $30,X          ; -> $0037


; -------------------------------------------------------
; Zero Page Wrap Around
; -------------------------------------------------------

    LDX #$10
    LDA #$77
    STA $F8,X          ; $F8+$10 -> $08

    LDY #$20
    LDX #$66
    STX $F0,Y          ; $F0+$20 -> $10

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
