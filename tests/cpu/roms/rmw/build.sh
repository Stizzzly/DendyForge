#!/bin/bash

set -e

ca65 rmw_test.asm -o rmw_test.o
ld65 rmw_test.o -C nes.cfg -o rmw_test.nes
