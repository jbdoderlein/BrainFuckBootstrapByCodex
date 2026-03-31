#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$repo_dir"

mkdir -p build

python3 tools/generate_bfc_bf.py

cc -O2 -std=c11 -Wall -Wextra -pedantic -o build/artifact1 src/bfc0.c

./build/artifact1 < src/bfc.bf > build/artifact2
chmod +x build/artifact2

./build/artifact2 < src/bfc.bf > build/artifact3
chmod +x build/artifact3

# Optional fixpoint check beyond the required three artifacts.
./build/artifact3 < src/bfc.bf > build/artifact3_fixpoint
chmod +x build/artifact3_fixpoint
cmp -s build/artifact3 build/artifact3_fixpoint
