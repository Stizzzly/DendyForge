#!/bin/bash

set -e

ca65 decimal_mode_test.asm -o decimal_mode_test.o
ld65 decimal_mode_test.o -C nes.cfg -o decimal_mode_test.nes
