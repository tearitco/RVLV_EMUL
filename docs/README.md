# RVLV_EMUL Documentation Hub

**Last Updated:** 2026-09-21  
**Repo Root:** `/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL`

---

## Quick Navigation

| Document | Purpose |
|----------|---------|
| [Architecture Decisions](ARCHITECTURE_DECISIONS.md) | Locked-in bootstrap architecture choices |
| [Progress Report](PROGRESS_REPORT.md) | Session progress with timestamps |
| [Quick Start](QUICK_START.md) | Reproduce the environment from scratch |
| [Directory Guide](DIRECTORY_GUIDE.md) | Every folder explained |
| [Agent Onboarding](AGENT_ONBOARDING.md) | How to pick up work |
| [Wins & Milestones](WINS.md) | What's working |
| [TODO & Blockers](TODO_BLOCKERS.md) | Current tasks and obstacles |

---

## Project Overview

```
RVLV_EMUL/
├── rvemu/                    # RISC-V emulator (QEMU virt) - WORKING
│   ├── build/main            # Built binary
│   ├── src/                  # C source (15 files)
│   └── includes/             # Headers
├── 0.hdl0+prisc-bootstrap/   # Bootstrap root
│   ├── prisc+x.c             # PAL VM (770 lines, POSIX)
│   └── 0.🧿️halo.c_spx=HDLb0/  # Hardware layer (NAND → RV32I)
├── xv6-riscv/                # Patched xv6 kernel (git submodule)
├── buildroot/                # Buildroot build (Linux images)
└── tests/                    # Test programs
```

**Three-layer architecture:**
1. **HDLb0** — Hardware simulation (NAND gates → RV32I CPU)
2. **rvemu** — Fast RISC-V emulator (execution layer)
3. **prisc+x** — Custom VM + ISA (software bridge for compiler tools)

---

## Current Status (2026-09-21)

| Component | Status |
|-----------|--------|
| rvemu self-test | ✅ PASS |
| rvemu syscall ABI | ✅ Implemented (write, read, open, close, fstat, lseek, brk, exit) |
| Buildroot Linux | ✅ Built (Image + rootfs.ext2) |
| Linux boot on rvemu | 🔄 Boots to userspace, virtio_blk probe fails |
| xv6-riscv | ✅ Boots (slow — memset in kfree) |
| prisc+x.c on Linux | ⏳ Next: compile & run |
| prisc+x_bare on rvemu | ⏳ Next: clean rewrite + musl |

---

## Next Immediate Steps

1. **Fix virtio_blk** in rvemu — Linux kernel probes but fails version check
2. **Compile prisc+x.c** as Linux binary, run on rvemu Linux
3. **Cross-compile musl** for `riscv64-unknown-elf`
4. **Write prisc+x_bare.c** (clean rewrite, ~300 lines, no POSIX)
5. **Run prisc+x_bare** as bare-metal ELF on rvemu

---

## Key Commands

```bash
# Self-test
./build/main --selftest

# xv6 to shell (~30 sec with kfree patch)
./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000

# Linux boot (current)
./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2 --max-inst 500000000

# Full test suite
./test_emulator.sh
```

---

## Repo Locations

| Repo | Path |
|------|------|
| Main working dir | `/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL` |
| Documentation | `/home/jb/Desktop/ele-desk+cal/1.RVEM/docs` |
| Buildroot output | `/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL/buildroot/output/images` |

---

*Start with [Quick Start](QUICK_START.md) to reproduce, then [Agent Onboarding](AGENT_ONBOARDING.md) for workflow.*