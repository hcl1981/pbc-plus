// =============================================================================
//  tusb_config_dualrole.h
//  Eigene TinyUSB-Konfiguration fuer i2collection.
//
//  Zweck: Device UND Host gleichzeitig einkompilieren, damit die USB-Rolle
//  ZUR LAUFZEIT (Menue HOST/JOIN) auf dem nativen Port gewaehlt werden kann.
//  Die Standard-Adafruit-Config (tusb_config_rp2040.h) schliesst das aus
//  (USE_TINYUSB_HOST => CFG_TUD_ENABLED 0). Diese Datei wird per Build-Flag
//  -DCFG_TUSB_CONFIG_FILE="tusb_config_dualrole.h" eingeschleust.
// =============================================================================
#ifndef _TUSB_CONFIG_DUALROLE_H_
#define _TUSB_CONFIG_DUALROLE_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// DUAL ROLE auf nativem Controller (Rootport 0)
//--------------------------------------------------------------------
#define CFG_TUD_ENABLED     1     // Device-Stack vorhanden
#define CFG_TUH_ENABLED     1     // Host-Stack vorhanden
#define CFG_TUH_RPI_PIO_USB 0     // Host laeuft nativ (kein PIO-USB)
// Kein CFG_TUH_MAX3421 -> kein externer Host-Controller.

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN TU_ATTR_ALIGNED(4)

//--------------------------------------------------------------------
// Device Configuration
//--------------------------------------------------------------------
#define CFG_TUD_ENDPOINT0_SIZE 64

#ifndef CFG_TUD_CDC
#define CFG_TUD_CDC 1
#endif
#ifndef CFG_TUD_MSC
#define CFG_TUD_MSC 0
#endif
#ifndef CFG_TUD_HID
#define CFG_TUD_HID 0
#endif
#ifndef CFG_TUD_MIDI
#define CFG_TUD_MIDI 0
#endif
#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR 0
#endif

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE 512
#define CFG_TUD_CDC_TX_BUFSIZE 512

//--------------------------------------------------------------------
// Host Configuration
//--------------------------------------------------------------------
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// keine Hubs noetig (Punkt-zu-Punkt)
#define CFG_TUH_HUB 0
#define CFG_TUH_DEVICE_MAX 1

#define CFG_TUH_CDC 1
#define CFG_TUH_CDC_FTDI 0
#define CFG_TUH_CDC_CP210X 0
#define CFG_TUH_CDC_CH34X 0

#define CFG_TUH_MSC 1   // muss 1 sein: Adafruit_USBH_CDC.cpp haengt (kurioserweise)
                        // hinter "#if CFG_TUH_ENABLED && CFG_TUH_MSC"
#define CFG_TUH_HID 0
#define CFG_TUH_MIDI 0

// RX & TX fifo size
#define CFG_TUH_CDC_RX_BUFSIZE 512
#define CFG_TUH_CDC_TX_BUFSIZE 512

// WICHTIG: KEINE Control-Transfers waehrend der Enumeration.
// Der ACM-Host-Treiber wuerde sonst SET_CONTROL_LINE_STATE/SET_LINE_CODING
// an das Geraet schicken und erst nach deren Abschluss mounten. Ueber diesen
// nicht-konformen Punkt-zu-Punkt-Link haengt genau dieser Schritt -> kein
// Mount. Der cdc_host-Treiber prueft die beiden Optionen per #ifdef, daher
// werden sie hier bewusst GAR NICHT definiert -> er faellt direkt auf
// "config complete" durch und mountet sofort.
// (weder CFG_TUH_CDC_LINE_CONTROL_ON_ENUM noch CFG_TUH_CDC_LINE_CODING_ON_ENUM)

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_DUALROLE_H_ */
