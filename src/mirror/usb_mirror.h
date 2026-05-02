#pragma once
// USB Mirror - Firmware-side USB-C LVGL mirror interface

#ifndef ENABLE_USB_UI_MIRROR
#define ENABLE_USB_UI_MIRROR 0
#endif

#ifndef USB_MIRROR_SERIAL_BAUD
#if ENABLE_USB_UI_MIRROR
#define USB_MIRROR_SERIAL_BAUD 2000000
#else
#define USB_MIRROR_SERIAL_BAUD 115200
#endif
#endif

#include "usb_mirror_protocol.h"
#include <lvgl.h>

void usb_mirror_begin();
void usbMirrorTask(void* pvParameters);

bool usb_mirror_enqueue_rect(const lv_area_t* area, const uint8_t* px_map);
void usb_mirror_read_pointer(lv_indev_data_t* data);

bool usb_mirror_is_armed();
void usb_mirror_set_armed(bool armed);
bool usb_mirror_is_connected();
uint32_t usb_mirror_drop_count();
