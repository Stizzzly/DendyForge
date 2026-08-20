#!/bin/bash

set -e

ca65 stack_test.asm -o stack_test.o
ld65 stack_test.o -C nes.cfg -o stack_test.nes
