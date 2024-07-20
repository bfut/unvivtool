#!/bin/bash

SCRIPT_PATH="${0%/*}"
cd $SCRIPT_PATH

mkdir .bin

ROOT="../.."

SRC="unvivtool.c"
DEST="unvivtool"


# compiler-specific
CPPFLAGS="-D_GLIBCXX_ASSERTIONS -fPIE -fstack-clash-protection -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2"

BRANCHES="-DUVTVUTF8"

# debug
GCCDEBUGFLAGS="-pedantic-errors -g -Wall -Wextra -Wstack-protector -fasynchronous-unwind-tables" # -fsanitize=leak
CPPDEBUGFLAGS="-g -Wall -Wextra -Wstack-protector -fasynchronous-unwind-tables" # -fsanitize=leak
GDB="-Og"

echo g++
g++ -std=c++17 $CPPFLAGS $ROOT/$SRC -o .bin/$DEST"_c++" ${BRANCHES}
echo clang++
clang++ -std=c++17 $CPPFLAGS $ROOT/$SRC -o .bin/$DEST"_clang++" ${BRANCHES}

