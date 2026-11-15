/* pbc_version.h -- the version of PBCemu, as the launcher's settings card
 * shows it and as two devices in link play tell each other.
 *
 * MAJOR.MINOR. Count MINOR up with every release, MAJOR with a big step
 * (MINOR then starts at 0 again). Each fits four bits: up to 15.15.
 * Firmwares from before 1.0 have no number.
 */
#ifndef PBC_VERSION_H
#define PBC_VERSION_H

#define PBCEMU_VERSION_MAJOR 1
#define PBCEMU_VERSION_MINOR 0

#define PBCEMU_VERSION_STR_(a, b) #a "." #b
#define PBCEMU_VERSION_STR__(a, b) PBCEMU_VERSION_STR_(a, b)
#define PBCEMU_VERSION PBCEMU_VERSION_STR__(PBCEMU_VERSION_MAJOR, PBCEMU_VERSION_MINOR)

/* one byte, as it travels: 0 means "before 1.0" */
#define PBCEMU_VERSION_BYTE ((PBCEMU_VERSION_MAJOR << 4) | PBCEMU_VERSION_MINOR)

#endif
