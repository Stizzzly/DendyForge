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

; =====================================
; ASL Accumulator
; =====================================

    LDA #$40
    ASL

; A = $80
; C = 0
; Z = 0
; N = 1

; =====================================
; ASL Accumulator (Carry)
; =====================================

    LDA #$80
    ASL

; A = $00
; C = 1
; Z = 1
; N = 0

; =====================================
; ASL Zero Page
; =====================================

    LDA #$01
    STA $10

    ASL $10

; [$10] = $02

; =====================================
; ASL Zero Page,X
; =====================================

    LDX #$01

    LDA #$04
    STA $11

    ASL $10,X

; [$11] = $08

; =====================================
; ASL Absolute
; =====================================

    LDA #$08
    STA $0200

    ASL $0200

; [$0200] = $10

; =====================================
; ASL Absolute,X
; =====================================

    LDX #$01

    LDA #$10
    STA $0201

    ASL $0200,X

; [$0201] = $20

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
