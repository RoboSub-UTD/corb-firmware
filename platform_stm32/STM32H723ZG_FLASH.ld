/* STM32H723ZG: 1 MB flash. Code, .data/.bss and the FreeRTOS heap live in AXI SRAM
 * (conservatively sized at 128 KB); the main/ISR stack (all of DTCM, 128 KB) sits in DTCM. */
ENTRY(Reset_Handler)

_estack = ORIGIN(DTCM) + LENGTH(DTCM);

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  DTCM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  RAM   (xrw) : ORIGIN = 0x24000000, LENGTH = 128K
}

SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text) *(.text*) *(.glue_7) *(.glue_7t) *(.eh_frame)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata : { . = ALIGN(4); *(.rodata) *(.rodata*) . = ALIGN(4); } >FLASH

  .ARM.extab : { . = ALIGN(4); *(.ARM.extab* .gnu.linkonce.armextab.*) . = ALIGN(4); } >FLASH
  .ARM : { . = ALIGN(4); __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; . = ALIGN(4); } >FLASH

  .preinit_array : { . = ALIGN(4); PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); . = ALIGN(4); } >FLASH
  .init_array : { . = ALIGN(4); PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); . = ALIGN(4); } >FLASH
  .fini_array : { . = ALIGN(4); PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); . = ALIGN(4); } >FLASH

  _sidata = LOADADDR(.data);

  .data : { . = ALIGN(4); _sdata = .; *(.data) *(.data*) . = ALIGN(4); _edata = .; } >RAM AT> FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .; __bss_start__ = _sbss;
    *(.bss) *(.bss*) *(COMMON)
    . = ALIGN(4);
    _ebss = .; __bss_end__ = _ebss;
  } >RAM

  /* newlib's malloc (unused by us, but nosys' _sbrk wants `end`). */
  ._user_heap_stack (NOLOAD) : { . = ALIGN(8); PROVIDE(end = .); PROVIDE(_end = .); . = . + 0x100; . = ALIGN(8); } >RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
