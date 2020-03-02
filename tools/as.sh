#!/bin/bash

# Get the hexadecimal instruction encoding of the
# given (assembly) instruction

if [[ $# -ne 1 ]]; then
    echo "Usage: $(basename "$0") <asm_insn>"
fi

file=$(mktemp)
out=$(echo "$1" | as -march=armv8.6-a -mcpu=all -mfpu=crypto-neon-fp-armv8 -o "$file" 2>&1)

if [[ $? -eq 1 ]]; then
    echo "$out" | tail -n1 | cut -d' ' -f3-
    exit 1
fi

objdump -d "$file" | tail -n1 | awk '{print $2}'
rm $file
