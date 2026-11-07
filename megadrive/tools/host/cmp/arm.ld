ENTRY(_start)
MEMORY { RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 48M }
SECTIONS {
  .text : { *(.text._start) *(.text*) *(.time_critical*) *(.rodata*) *(.data*) . = ALIGN(8); } > RAM
  .bss (NOLOAD) : { __bss_start__ = .; *(.bss*) *(COMMON) . = ALIGN(8); __bss_end__ = .; } > RAM
  end = .;
  /DISCARD/ : { *(.ARM.exidx*) }
}
