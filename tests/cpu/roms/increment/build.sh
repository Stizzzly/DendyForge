#!/bin/bash

set -e

echo "Assembling..."
ca65 increment_test.asm -o increment_test.o

echo "Linking..."
ld65 increment_test.o -C nes.cfg -o increment_test.nes

echo "Done!"
