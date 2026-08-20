#!/bin/bash

set -e

echo "Assembling..."
ca65 transfer_test.asm -o transfer_test.o

echo "Linking..."
ld65 transfer_test.o -C nes.cfg -o transfer_test.nes

echo "Done!"
