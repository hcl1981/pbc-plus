/* pbc_devices.h -- the systems PBCemu offers, in the launcher's order.
 *
 * The launch record (pbc_launch.h) names the device by its index in this
 * list, so the launcher (pbcemu/src/devices.c) and the emulators' loading
 * screen (pbc_loading.c) expand the same list. Each entry:
 *   X(name, maker, extensions, app, accent, picture, icon, controls, modes)
 * The last entry is "Add games" (USB). In order of their release.
 */
#ifndef PBC_DEVICES_LIST_H
#define PBC_DEVICES_LIST_H

#define PBC_DEVICES(X) \
    X("Commodore 64",   "Commodore  \xfa  1982", ".d64.prg",        APP_C64,  0xd9a55b, pic_c64, icon_c64, c_c64,  m_c64) \
    X("NES",            "Nintendo  \xfa  1983",  ".nes",            APP_NES,  0xe5483c, pic_nes, icon_nes, c_nes,  m_nes) \
    X("Master System",  "Sega  \xfa  1985",      ".sms",            APP_SMS,  0x3d8bfd, pic_sms, icon_sms, c_sms,  m_sms) \
    X("PC Engine",      "NEC  \xfa  1987",       ".pce",            APP_PCE,  0xff8a3d, pic_pce, icon_pce, c_pce,  m_pce) \
    X("Mega Drive",     "Sega  \xfa  1988",      ".md.gen.smd.bin", APP_MD,   0xe0407a, pic_md,  icon_md,  c_md,   m_md)  \
    X("Game Gear",      "Sega  \xfa  1990",      ".gg",             APP_SMS,  0x2cc6d8, pic_gg,  icon_gg,  c_gg,   m_gg)  \
    X("Game Boy Color", "Nintendo  \xfa  1998",  ".gbc.gb",         APP_GBC,  0xa56cf5, pic_gbc, icon_gbc, c_gbc,  m_gbc) \
    X("Add Games",      "USB drive",             "",                APP_NONE, 0x4ea1ff, pic_usb, icon_usb, c_none, NULL)

#endif
