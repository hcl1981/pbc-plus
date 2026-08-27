/*
 * usb_descriptors.c -- USB descriptors for the PicoBoyGB mass-storage device.
 *
 * A single MSC (bulk-only) interface exposing the on-board FatFs partition as
 * a USB stick called "PICOBOYGB". Self-contained (no TinyUSB BSP board layer):
 * the serial string is derived from the RP2350 unique flash id.
 */

#include "tusb.h"
#include "pico/unique_id.h"
#include <string.h>

#define USB_VID   0xCafe
#define USB_PID   0x4002   /* MSC-only product id */

//--------------------------------------------------------------------
// Device descriptor
//--------------------------------------------------------------------
static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------
// Configuration descriptor
//--------------------------------------------------------------------
enum { ITF_NUM_MSC, ITF_NUM_TOTAL };

#define EPNUM_MSC_OUT   0x01
#define EPNUM_MSC_IN    0x81

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

//--------------------------------------------------------------------
// String descriptors
//--------------------------------------------------------------------
enum { STRID_LANGID, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL };

static const char *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },   // 0: language = English (0x0409)
    "PicoBoy",                      // 1: Manufacturer
    "PicoBoyGB",                    // 2: Product
    NULL,                           // 3: Serial (filled from unique id)
};

static uint16_t _desc_str[32 + 1];

static size_t board_serial_utf16(uint16_t *dst, size_t max_chars) {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    for (size_t i = 0; i < sizeof(id.id) && n + 2 <= max_chars; ++i) {
        dst[n++] = hex[(id.id[i] >> 4) & 0xF];
        dst[n++] = hex[id.id[i] & 0xF];
    }
    return n;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    switch (index) {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;
        case STRID_SERIAL:
            chr_count = board_serial_utf16(_desc_str + 1, 32);
            break;
        default:
            if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
            const char *str = string_desc_arr[index];
            if (!str) return NULL;
            chr_count = strlen(str);
            const size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
            if (chr_count > max_count) chr_count = max_count;
            for (size_t i = 0; i < chr_count; ++i) _desc_str[1 + i] = str[i];
            break;
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
