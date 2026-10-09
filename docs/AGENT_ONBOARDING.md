# Agent Onboarding — How to Pick Up Work

**For:** Any agent (human or AI) joining the RVLV_EMUL project  
**Read time:** 5 min

---

## 1. Read These First (in order)

| Document | Why |
|----------|-----|
| [Architecture Decisions](../ARCHITECTURE_DECISIONS.md) | Locked-in bootstrap architecture — don't re-debate |
| [Progress Report](PROGRESS_REPORT.md) | What's done, what's running, what's next |
| [Directory Guide](DIRECTORY_GUIDE.md) | Where everything lives |
| [Quick Start](QUICK_START.md) | How to reproduce the environment |

---

## 2. Current Sprint (This Week)

### Goal: prisc+x running on rvemu (bare-metal)

| Task | Status | Owner |
|------|--------|-------|
| rvemu syscall ABI | ✅ Done | — |
| Buildroot Linux images | ✅ Done | — |
| Linux boot on rvemu | 🔄 Boots, virtio_blk fails | — |
| Fix virtio_blk version | 🔴 Blocker | Next |
| Compile prisc+x.c as Linux binary | ⏳ Ready | Next |
| Cross-compile musl for RISC-V | ⏳ Ready | Next |
| Write prisc+x_bare.c (clean rewrite) | ⏳ Ready | Next |
| Run prisc+x_bare on rvemu | ⏳ Ready | Next |

---

## 3. Workflow Rules

### Do
- ✅ Update `PROGRESS_REPORT.md` with timestamps
- ✅ Update `TODO_BLOCKERS.md` when status changes
- ✅ Commit working code before major changes
- ✅ Run `./build/main --selftest` after any rvemu change
- ✅ Ask user before architectural decisions

### Don't
- ❌ Re-debate locked decisions (see ARCHITECTURE_DECISIONS.md)
- ❌ Add Rust, Python, or new languages to bootstrap chain
- ❌ Modify prisc+x.c with `#ifdef BARE_METAL` — clean rewrite only
- ❌ Commit Buildroot output/ or dl/ directories

---

## 4. Key Commands Reference

```bash
# Build rvemu (after any src/ change)
cd RVLV_EMUL && ./build.sh

# Self-test (must pass)
./build/main --selftest

# Test xv6
./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000

# Test Linux (current best)
./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2 --max-inst 500000000

# Full test suite
./test_emulator.sh

# Rebuild Buildroot (if config changed)
cd buildroot && make -j$(nproc)
```

---

## 5. Debugging rvemu

| Issue | Debug Flag |
|-------|------------|
| Instruction trace | `--dump` |
| Trap/interrupt trace | `--trace-trap` |
| Stop after N instructions | `--max-inst N` |
| Boot Linux with SBI | `--linux` |

**Example:**
```bash
./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2 --max-inst 1000000 --trace-trap --dump
```

---

## 6. Common Fixes & Patterns

### rvemu hangs at PC=0
- Check `MTVEC` is set correctly
- Check trap delegation (`MIDELEG`, `MEDELEG`)
- See `INVESTIGATION_REPORT.md` for Bug 3 (irq_enabled fix)

### Linux kernel panic / no rootfs
- Check `bootargs` in `main.c:41`
- Check VirtIO disk probe (`virtio.c`)
- Try `--virtio-legacy` for old xv6

### Self-test fails
- Run with `--trace-trap --dump` to see failing instruction
- Compare with `selftest.c` expectations

---

## 6. Princples for This Project

1. **Small, auditable trusted base** — prisc+x_bare.c must be <400 lines
2. **No hidden complexity** — no `#ifdef` soup, no magic
3. **Bash/C/prisc+x only** — no Rust, Go, Python in bootstrap chain
4. **Reproducible** — every step documented in Quick Start
5. **Incremental verification** — each layer tests the one below

---

## 7. Where to Ask Questions

- **Architecture:** Check `ARCHITECTURE_DECISIONS.md` first
- **Current status:** Check `PROGRESS_REPORT.md`
- **Blockers:** Check `TODO_BLOCKERS.md`
- **User:** Ask directly — don't assume

---

## 8. Session Handoff Template

When ending session, update:
1. `PROGRESS_REPORT.md` — timestamps, status
2. `TODO_BLOCKERS.md` — what's blocked, what's ready
3. Commit any working code
4. Leave a 3-bullet summary for next agent

---

*Welcome to the bootstrap. Every layer proves the one below.*