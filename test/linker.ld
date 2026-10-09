SECTIONS
{
  . = 0x80000000;
  .text : {
    *(.text .text.*)
    *(.init)
  }
  .rodata : {
    *(.rodata .rodata.*)
    *(.srodata .srodata.*)
  }
  .bss : {
    *(.bss .bss.*)
    *(.sbss .sbss.*)
    *(COMMON)
  }
  _stack_top = . + 0x1000;
}
