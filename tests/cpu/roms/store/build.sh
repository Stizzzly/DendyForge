#!/bin/bash

set -e

echo "Assembling..."
ca65 store_test.asm -o store_test.o

echo "Linking..."
ld65 store_test.o -C nes.cfg -o store_test.nes

echo "Done!"
