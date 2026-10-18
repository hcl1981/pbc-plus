/* d64_dir.h -- list the programs on a .d64 disk image (menu only). */
#ifndef PBC_D64_DIR_H
#define PBC_D64_DIR_H

#define D64_NAME_MAX 16             /* CBM DOS file names are up to 16 bytes */

/* Reads the directory of `path` (FatFs, mounted) and copies the names of
 * all closed PRG files, unpadded, as raw PETSCII. Returns their number. */
int d64_list_prg(const char *path, char names[][D64_NAME_MAX + 1], int max);

#endif /* PBC_D64_DIR_H */
