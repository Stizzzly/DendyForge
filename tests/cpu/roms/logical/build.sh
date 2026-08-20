#!/bin/bash

set -e

echo "Assembling..."
ca65 logical_test.asm -o logical_test.o

echo "Linking..."
ld65 logical_test.o -C nes.cfg -o logical_test.nes

echo "Done!"
