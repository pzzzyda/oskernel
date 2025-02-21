OUTPUT_ARCH("riscv")
ENTRY(_entry)

SECTIONS
{
  . = 0x80200000;

  .text : {
    PROVIDE(_text_start = .);
    *(.text .text.*)
    . = ALIGN(0x1000);
    *(trampsec)
    . = ALIGN(0x1000);
    PROVIDE(_text_end = .);
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*)
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*)
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  . = ALIGN(0x1000);
  PROVIDE(_kernel_end = .);
}
