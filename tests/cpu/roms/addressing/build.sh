#!/bin/bash

set -e

ca65 addressing_test.asm -o addressing_test.o
ld65 addressing_test.o -C nes.cfg -o addressing_test.nes
