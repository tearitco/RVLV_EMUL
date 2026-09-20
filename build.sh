#!/usr/bin/env bash
# build.sh - direct GCC build for rvemu (no CMake/Ninja)
set -euo pipefail

CC="${CC:-gcc}"
CFLAGS="-O2 -std=gnu2x -Wall -Wextra -Werror -Iincludes -DNDEBUG"
# Compatibility shims for GCC < 12 (no C23 nullptr):
CFLAGS="${CFLAGS} -Dnullptr=0 -Wno-error=format"
LDFLAGS="-lm"
BUILDDIR="${BUILDDIR:-build}"
APP="${1:-${BUILDDIR}/main}"

SRCS=(main.c src/bus.c src/clint.c src/cpu.c src/csr.c src/dram.c src/dtb.c
      src/fpu.c src/mmu.c src/plic.c src/sbi.c src/selftest.c src/trap.c
      src/uart.c src/virtio.c)

mkdir -p "${BUILDDIR}"

OBJS=()
for src in "${SRCS[@]}"; do
    obj="${BUILDDIR}/${src%.c}.o"
    mkdir -p "$(dirname "${obj}")"
    echo "  CC ${src}"
    "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
    OBJS+=("${obj}")
done

echo "  LD ${APP}"
"${CC}" ${CFLAGS} "${OBJS[@]}" -o "${APP}" ${LDFLAGS}
echo "build ok: ${APP}"
