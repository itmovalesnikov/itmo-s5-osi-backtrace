#!/bin/sh
set -eu

if [ "$#" -lt 1 ]; then
  printf 'Usage: sh symbolize.sh ELF [ADDRESS ...]\n' >&2
  exit 1
fi

elf=$1
shift
tool=${ADDR2LINE:-riscv64-elf-addr2line}

if [ "$#" -gt 0 ]; then
  printf '%s\n' "$@" | "$tool" -a -f -C -e "$elf"
else
  "$tool" -a -f -C -e "$elf"
fi