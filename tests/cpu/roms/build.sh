#!/bin/bash

set -e

echo "Assembling..."
ca65 cpu_test.asm -o cpu_test.o

echo "Linking..."
ld65 cpu_test.o -C nes.cfg -o cpu_test.nes

echo "Done!"
