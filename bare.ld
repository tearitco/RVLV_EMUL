/* Linker script for bare-metal RISC-V at DRAM_BASE (0x80000000) */
ENTRY(_start)
SECTIONS
{
    . = 0x80000000;
    .text : {
        *(.text._start)
        *(.text)
        *(.rodata*)
    }
    .data : {
        *(.data)
        *(.bss)
    }
    /DISCARD/ : {
        *(.eh_frame*)
        *(.comment)
    }
}