#!/bin/bash

set -e

ca65 interrupts_test.asm -o interrupts_test.o
ld65 interrupts_test.o -C nes.cfg -o interrupts_test.nes
