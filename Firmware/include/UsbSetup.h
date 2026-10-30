#pragma once
#include <Adafruit_TinyUSB.h>

// USB複合デバイス: HID(キーボード + メディアキー) / MSC(設定ページ) / CDC(Serial)
extern Adafruit_USBD_HID usb_hid;
extern Adafruit_USBD_MSC usb_msc;

// PCへのUSB検出を一度切断する (構成を組み立て終わるまでPCに見せない)
void usbDetach();

// VID/PID・各インターフェースを設定し、Serialを開始して、PCへ接続通知(attach)を出す
void usbBegin();

// いずれかのUSBインターフェースが有効になるまで待つ
void usbWaitReady();

// 指定ミリ秒の間 tud_task() を回し続ける
void usbPump(uint32_t ms);
