#!/bin/bash

set -e

echo "Assembling..."
ca65 compare_test.asm -o compare_test.o

echo "Linking..."
ld65 compare_test.o -C nes.cfg -o compare_test.nes

echo "Done!"
