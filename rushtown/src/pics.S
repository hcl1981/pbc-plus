/* pics.S -- the story's pictures, as tools/art.py packed them (pics.bin). */
#if !defined(RT_ART) || RT_ART
    .section .rodata
    .global pics_blob
    .balign 4
pics_blob:
    .incbin "pics.bin"
#endif
#if defined(__linux__) && defined(__ELF__)
    .section .note.GNU-stack, "", %progbits
#endif
