#!/bin/bash

set -e

echo "==========================="
echo "Building Dendy test ROMs"
echo "==========================="

for file in *.asm
do
    name="${file%.asm}"

    echo
    echo "Assembling ${name}.asm"

    ca65 "${file}" -o "${name}.o"

    echo "Linking ${name}.nes"

    ld65 \
        -C nes.cfg \
        "${name}.o" \
        -o "${name}.nes"

    rm "${name}.o"

done

echo
echo "Done."
