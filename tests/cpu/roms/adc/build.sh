#!/bin/bash

set -e

echo "Assembling..."
ca65 adc_carry_in.asm -o adc_carry_in.o

echo "Linking..."
ld65 adc_carry_in.o -C nes.cfg -o adc_carry_in.nes

echo "Done!"
