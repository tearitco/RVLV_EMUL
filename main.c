#define _GNU_SOURCE
#include "cpu.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
    fprintf(stderr,
            "Usage: main [options] [binary]\n"
            "  -k, --kernel FILE     kernel or bare-metal image (ELF or raw)\n"
            "  -b, --bios FILE       M-mode firmware (OpenSBI / BBL)\n"
            "  -f, --disk FILE       virtio-blk disk image\n"
            "  -i, --initrd FILE     initrd (Linux)\n"
            "      --dtb FILE        load a DTB instead of generating one\n"
            "      --linux           S-mode Linux boot with built-in SBI + DTB\n"
            "      --bootargs STR    Linux bootargs (default: console=ttyS0 root=/dev/vda rw)\n"
            "      --virtio-legacy   virtio MMIO version 1 (old xv6)\n"
            "  -m, --mem MIB         DRAM size in MiB (default 128)\n"
            "      --dump            print PC and registers each instruction\n"
            "      --trace-trap      print exceptions and interrupts\n"
            "      --max-inst N      stop after N instructions\n"
            "      --selftest        run built-in ISA checks\n");
}

static int eq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

int main(int argc, char **argv)
{
    const char *kernel = nullptr;
    const char *bios = nullptr;
    const char *disk = nullptr;
    const char *initrd = nullptr;
    const char *dtb_file = nullptr;
    const char *bootargs = "console=ttyS0,115200n8 earlycon root=/dev/vda rw rootwait";
    int linux_boot = 0;
    int virtio_legacy = 0;
    int dump = 0;
    int trace_trap = 0;
    uint64_t mem_mib = DEFAULT_DRAM_MIB;
    uint64_t max_inst = 0;

    for (int i = 1; i < argc; i++) {
        if (eq(argv[i], "--selftest"))
            return selftest();
        if (eq(argv[i], "-h") || eq(argv[i], "--help")) {
            usage();
            return 0;
        }
        if ((eq(argv[i], "-k") || eq(argv[i], "--kernel")) && i + 1 < argc)
            kernel = argv[++i];
        else if ((eq(argv[i], "-b") || eq(argv[i], "--bios")) && i + 1 < argc)
            bios = argv[++i];
        else if ((eq(argv[i], "-f") || eq(argv[i], "--disk")) && i + 1 < argc)
            disk = argv[++i];
        else if ((eq(argv[i], "-i") || eq(argv[i], "--initrd")) && i + 1 < argc)
            initrd = argv[++i];
        else if (eq(argv[i], "--dtb") && i + 1 < argc)
            dtb_file = argv[++i];
        else if (eq(argv[i], "--bootargs") && i + 1 < argc)
            bootargs = argv[++i];
        else if (eq(argv[i], "--linux"))
            linux_boot = 1;
        else if (eq(argv[i], "--virtio-legacy"))
            virtio_legacy = 1;
        else if (eq(argv[i], "--dump"))
            dump = 1;
        else if (eq(argv[i], "--trace-trap"))
            trace_trap = 1;
        else if ((eq(argv[i], "-m") || eq(argv[i], "--mem")) && i + 1 < argc) {
            mem_mib = strtoull(argv[++i], nullptr, 0);
        } else if (eq(argv[i], "--max-inst") && i + 1 < argc)
            max_inst = strtoull(argv[++i], nullptr, 0);
        else if (argv[i][0] != '-' && !kernel)
            kernel = argv[i];
        else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (!kernel && !bios) {
        usage();
        return 1;
    }
    if (mem_mib == 0 || mem_mib > MAX_DRAM_MIB) {
        fprintf(stderr, "DRAM size must be 1..%" PRIu64 " MiB\n", MAX_DRAM_MIB);
        return 1;
    }

    CPU cpu;
    if (cpu_init(&cpu, mem_mib * 1024ull * 1024ull, disk, virtio_legacy) < 0) {
        fprintf(stderr, "failed to initialize emulator (DRAM/disk)\n");
        return 1;
    }
    cpu.dump = dump;
    cpu.trace_trap = trace_trap;

    uint64_t entry = DRAM_BASE;
    uint64_t ksize = 0;
    if (bios) {
        uint64_t bentry = DRAM_BASE;
        if (load_image(&cpu, bios, DRAM_BASE, &bentry, nullptr) < 0) {
            cpu_destroy(&cpu);
            return 1;
        }
        entry = bentry;
        if (kernel) {
            if (load_image(&cpu, kernel, 0x80200000ull, nullptr, &ksize) < 0) {
                cpu_destroy(&cpu);
                return 1;
            }
        }
    } else if (kernel) {
        if (load_image(&cpu, kernel, DRAM_BASE, &entry, &ksize) < 0) {
            cpu_destroy(&cpu);
            return 1;
        }
    }

    uint64_t initrd_start = 0, initrd_end = 0;
    if (initrd) {
        initrd_start = DRAM_BASE + cpu.bus.dram.size - 8 * 1024 * 1024ull;
        if (load_image(&cpu, initrd, initrd_start, &initrd_start, nullptr) < 0) {
            cpu_destroy(&cpu);
            return 1;
        }
        FILE *f = fopen(initrd, "rb");
        if (!f) {
            cpu_destroy(&cpu);
            return 1;
        }
        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            cpu_destroy(&cpu);
            return 1;
        }
        long n = ftell(f);
        fclose(f);
        if (n < 0) {
            cpu_destroy(&cpu);
            return 1;
        }
        initrd_end = initrd_start + (uint64_t)n;
    }

    uint64_t dtb_addr = DRAM_BASE + cpu.bus.dram.size - 2 * 1024 * 1024ull;
    int need_dtb = linux_boot || bios || dtb_file;
    if (dtb_file) {
        uint64_t e = dtb_addr;
        if (load_image(&cpu, dtb_file, dtb_addr, &e, nullptr) < 0) {
            cpu_destroy(&cpu);
            return 1;
        }
        dtb_addr = e;
    } else if (need_dtb) {
        uint64_t dtsz = 0;
        if (generate_dtb(&cpu, cpu.bus.dram.size, initrd_start, initrd_end, bootargs, dtb_addr,
                         &dtsz) < 0) {
            fprintf(stderr, "failed to generate DTB\n");
            cpu_destroy(&cpu);
            return 1;
        }
    }

    cpu.pc = entry;
    cpu.regs[10] = 0;
    cpu.regs[11] = need_dtb ? dtb_addr : 0;
    cpu.dtb_addr = dtb_addr;
    cpu.entry = entry;
    if (linux_boot && !bios) {
        cpu.priv = PRIV_S;
        cpu.sbi = 1;
        cpu.csr[MSTATUS] = (2ULL << 32) | (2ULL << 34) | MSTATUS_FS_INITIAL | MSTATUS_SPIE;
        cpu.csr[MIDELEG] = MIP_SSIP | MIP_STIP | MIP_SEIP;
        cpu.csr[MEDELEG] = 0xffff;
        cpu.csr[MENVCFG] = MENVCFG_STCE | MENVCFG_ADUE;
        cpu.csr[MCOUNTEREN] = 0x7;
    }

    uint64_t steps = 0;
    while (!cpu.halt) {
        cpu_step(&cpu);
        steps++;
        if (max_inst && steps >= max_inst) {
            fprintf(stderr,
                    "stopped pc=%" PRIx64 " priv=%u mstatus=%" PRIx64 " mcause=%" PRIx64
                    " satp=%" PRIx64 " mtvec=%" PRIx64 " stvec=%" PRIx64 " instret=%" PRIu64 "\n",
                    cpu.pc, cpu.priv, cpu.csr[MSTATUS], cpu.csr[MCAUSE], cpu.csr[SATP],
                    cpu.csr[MTVEC], cpu.csr[STVEC], cpu.instret);
            break;
        }
        if (cpu.pc == 0 && cpu.priv == PRIV_M && (cpu.csr[MTVEC] & ~3ull) == 0)
            break;
    }
    uart_restore_term();
    int code = cpu.halt_code;
    cpu_destroy(&cpu);
    return code;
}
