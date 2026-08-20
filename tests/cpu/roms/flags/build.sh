#!/bin/bash

set -e

echo "Assembling..."
ca65 flag_test.asm -o flag_test.o

echo "Linking..."
ld65 flag_test.o -C nes.cfg -o flag_test.nes

echo "Done!"
