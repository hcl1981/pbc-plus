/*
 * tusb_config.h -- TinyUSB configuration for PicoBoyGB (SDK build).
 *
 * Device stack, single MSC (mass storage) interface. That is all we need to
 * export the FatFs partition to the host as a USB stick for dropping .gb ROMs.
 * CFG_TUSB_MCU is supplied by the pico-sdk TinyUSB integration.
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// Board / RHPort
//--------------------------------------------------------------------
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// Common
//--------------------------------------------------------------------
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined (provided by the pico-sdk)
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__ ((aligned(4)))
#endif

// Enable device stack
#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

//--------------------------------------------------------------------
// Device configuration
//--------------------------------------------------------------------
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE   64
#endif

//------------- Classes -------------//
#define CFG_TUD_CDC              0
#define CFG_TUD_MSC              1
#define CFG_TUD_HID              0
#define CFG_TUD_MIDI             0
#define CFG_TUD_VENDOR           0

// Mass storage transfer buffer size. The host writes our 4096-byte sectors in
// chunks of this size (see usb_msc.c, which reassembles a full sector).
#define CFG_TUD_MSC_EP_BUFSIZE   512

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
