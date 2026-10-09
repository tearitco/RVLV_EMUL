CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

ARCH = rv32im
ABI = ilp32

CFLAGS = -march=$(ARCH) -mabi=$(ABI) -O2 -ffreestanding -nostdlib -nostartfiles
LDFLAGS = -T linker.ld -melf32lriscv

TESTS = $(patsubst %.S,%,$(wildcard *.S)) $(patsubst %.c,%,$(wildcard *.c))
BINS = $(addprefix $(BUILDDIR)/,$(addsuffix .bin,$(TESTS)))
ELFS = $(addprefix $(BUILDDIR)/,$(addsuffix .elf,$(TESTS)))

BUILDDIR = build

.PHONY: all clean run

all: $(BINS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.elf: %.S | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@.o
	$(LD) $(LDFLAGS) -o $@ $@.o

$(BUILDDIR)/%.elf: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@.o
	$(LD) $(LDFLAGS) -o $@ $@.o

$(BUILDDIR)/%.bin: $(BUILDDIR)/%.elf
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -d $< > $(@:.bin=.dis)
.PRECIOUS: $(BUILDDIR)/%.elf

clean:
	rm -rf $(BUILDDIR)

run: $(BINS)
	@echo "=== Running bare-metal tests ==="
	@for test in $(BINS); do \
		echo "Running $$test..."; \
		/tmp/rvemu $$test; \
		code=$$?; \
		if [ $$code -eq 0 ]; then \
			echo "  PASS"; \
		else \
			echo "  FAIL (exit code $$code)"; \
		fi; \
	done
