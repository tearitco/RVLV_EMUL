# RVLV_EMUL Progress Report

**Session Started:** 2026-09-20T06:40:28Z  
**Last Updated:** 2026-09-24T16:30:00Z
**Working Directory:** `/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL`

---

## ✅ Completed Tasks

| Time | Task | Status | Notes |
|------|------|--------|-------|
| 06:42 | Cloned RVLV_EMUL repo from GitHub | ✅ Done | `git clone https://github.com/tearitco/RVLV_EMUL.git` |
| 06:55 | Fixed `build_buildroot.sh` hardcoded paths | ✅ Done | Updated to use `$REPO_ROOT` instead of `/home/no/...` |
| 06:58 | Built rvemu via `./build.sh` | ✅ Done | All 15 source files compiled successfully |
| 07:00 | Ran rvemu self-test | ✅ **PASSED** | `./build/main --selftest` → "all checks passed" |
| 07:16 | Buildroot build completed | ✅ Done | Linux 6.18.7 + rootfs.ext2 built (3 hours) |
| 09:00 | Linux boot on rvemu | ✅ **WORKING** | Boots to userspace, virtio_blk detected |
| 09:30 | virtio_blk version fix | ✅ Done | Added VIRTIO_F_VERSION_1 feature bit |
| 10:00 | Syscall ABI in rvemu | ✅ Done | `src/syscall.c` + `src/cpu.c` - 8 syscalls |
| 10:30 | Self-test still passes | ✅ Done | All checks pass with syscall changes |
| 13:00 | prisc+x.c compiled on host | ✅ Done | VM interprets .pal files correctly |
| 13:30 | prisc+x VM verified | ✅ Done | Arithmetic, jumps, builtin_out work |
| **2026-09-22** | **musl cross-compile** | ✅ **DONE** | Static libc for RISC-V bare-metal, tested with musl-gcc |
| **2026-09-22** | **prisc+x on QEMU Linux** | ✅ **WORKING** | Compiled for Linux target, ran via 9p share, output: `OUT: -5` |
| **2026-09-23** | **GitHub push + docs** | ✅ **DONE** | Pushed to github.com/tearitco/RVLV_EMUL, ARCHITECTURE_DECISIONS.md updated |
| **2026-09-23** | **prisc+x_bare.c complete** | ✅ **DONE** | ~750 lines, all string ops, syscalls, custom ops, bare-metal |
| **2026-09-23** | **prisc+x_bare on rvemu** | ✅ **WORKING** | Compiles with buildroot GCC, runs bare-metal, exit code 0 |
| **2026-09-23** | **Custom spawn syscall** | ✅ **DONE** | SYS_SPAWN (400) in rvemu + prisc_bare builtin_spawn |
| **2026-09-23** | **Backup script** | ✅ **DONE** | `rezip-rvem.sh` creates 550KB .7z excluding build artifacts |

---

## 🔄 In Progress

### Minimal Linux Kernel Config (COMPLETE 2026-09-24)
- [x] Create `kernel/` directory with `.config`
- [x] Copy Linux 6.18.7 source from buildroot to `kernel/src/` (ref only)
- [x] Enable 25+ required kernel config options
- [x] Add serial console/earlycon drivers
- [x] Enable VIRTIO_MMIO + VIRTIO_BLK + VIRTIO_CONSOLE + VIRTIO_NET
- [x] Build minimal kernel Image (3.3 MB)
- [x] Boot on rvemu: UART output ✅, virtio-blk detected ✅, rootfs mounted ✅
- Note: Userspace SIGILL in dynamic linker (ISA extension mismatch; rootfs compatibility issue). Buildroot GCC targets `rv64imafd_zicsr_zifencei` but rvemu implements subset (no vector, limited CSR/ZIFENCEI). **Decision:** Skip ISA extension work for now — assembler only needs RV32I base instructions.

---

## 📋 Next Tasks (Priority Order)

| # | Task | Est. Time |
|---|------|-----------|
| 1 | RISC-V Assembler in prisc+x (`.pal` files) | 2-4 hrs |
| 2 | C Compiler in prisc+x (`.pal` files) | 1-2 weeks |
| 3 | Fix rootfs ISA compatibility for bare-metal userspace | 1-2 hrs |
| 4 | Self-hosting: Compiler compiles prisc+x_bare.c | 2-4 hrs |

---

## 📁 Key Files Status

| File | Status |
|------|--------|
| `./build/main` | ✅ Built & tested (with syscalls + virtio fix + spawn) |
| `./build.sh` | ✅ Working |
| `./build_buildroot.sh` | ✅ Fixed paths |
| `./xv6-riscv/` | ✅ Submodule present |
| `buildroot/output/images/Image` | ✅ Built (27 MB) |
| `buildroot/output/images/rootfs.ext2` | ✅ Built (60 MB) |
| `src/syscall.c` | ✅ Created (9 syscalls: +spawn) |
| `prisc+x.c` | ✅ Works on host Linux + QEMU Linux |
| `src/prisc_bare.c` | ✅ Complete (~750 lines, bare-metal) |
| `musl` cross-compile | ✅ Done (`musl-install/`) |
| `minimal Linux kernel` | ✅ Built & tested (`kernel/.config`, `kernel/Image`) |
| `kernel/.config` | ✅ Minimal 3.3 MB config |
| `rezip-rvem.sh` | ✅ Backup script working |

---

## 🎯 Session Goals (Updated)

1. ✅ Run `./install_deps.sh` equivalent (manual build done)
2. ✅ Buildroot Linux images built
3. ✅ Linux boot on rvemu (virtio_blk works)
4. ✅ Syscall ABI implemented (`src/syscall.c`)
5. ✅ prisc+x.c on Linux (compiled for Linux target, ran on QEMU via 9p)
6. ✅ Cross-compile musl for bare-metal RISC-V
7. ✅ Write `prisc+x_bare.c` clean rewrite
8. ✅ Run prisc+x bare-metal on rvemu
9. ✅ Build minimal Linux kernel for fast testing (M7 complete)
10. ⏳ RISC-V Assembler in prisc+x (`.pal` files) — NEXT

---

## 📝 Notes

- **rvemu syscalls:** write(64), read(63), open(1024), close(57), fstat(80), lseek(62), brk(214), exit(93), spawn(400)
- **SBI vs Linux syscall:** `a7 >= 0x100000` → SBI, else → Linux syscall
- **virtio_blk:** Added VIRTIO_F_VERSION_1 (bit 32) to device features
- **prisc+x VM:** 770 lines (C), 750 lines (bare), custom ISA, string ops, ECALL syscalls, custom ops (`+x/`)
- **Bootstrap architecture locked:** prisc+x = trusted base, assembler in prisc+x, clean bare-metal rewrite, no Stage0
- **Documentation created:** `docs/` with README, QUICK_START, DIRECTORY_GUIDE, AGENT_ONBOARDING, WINS, ARCHITECTURE_DECISIONS
- **Minimal Linux kernel:** Documented in ARCHITECTURE_DECISIONS.md — tinyconfig + virtio only, fork/exec for IPC, no header deps
- **In-house stack philosophy:** Every external tool (Buildroot, musl, QEMU) is scaffolding; in-house replacements planned for all layers
- **GitHub:** Pushed to https://github.com/tearitco/RVLV_EMUL (main branch)

---

*Report generated automatically. Update with `cat PROGRESS_REPORT.md` to view.*