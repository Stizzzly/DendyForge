#!/bin/bash

set -e

echo "Assembling..."
ca65 load_test.asm -o load_test.o

echo "Linking..."
ld65 load_test.o -C nes.cfg -o load_test.nes

echo "Done!"
