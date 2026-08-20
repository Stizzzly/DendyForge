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

; -----------------------------
; AND
; -----------------------------

    LDA #%11110000
    AND #%10101010

; -----------------------------
; ORA
; -----------------------------

    LDA #%11000000
    ORA #%00001111

; -----------------------------
; EOR
; -----------------------------

    LDA #%11111111
    EOR #%10101010

; -----------------------------
; BIT
; -----------------------------

    LDA #%01000000

    LDX #%00000000
    STX $10

    BIT $10

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
