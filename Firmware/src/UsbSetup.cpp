#include "UsbSetup.h"
#include <Arduino.h>
#include <string.h>

// ------------------------------------------------------------------
// HID レポート記述子（キーボード ID:1 + 音量/メディア ID:2）
// ------------------------------------------------------------------
static uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2))};

// USB インスタンス
Adafruit_USBD_MSC usb_msc;
Adafruit_USBD_HID usb_hid;

static bool msc_ready = false;

// ------------------------------------------------------------------
// MSC(USBメモリ)に見せる 8KB(16セクタ) の FAT12 ボリューム
//   LBA0: ブートセクタ / LBA1,2: FAT / LBA3: ルートディレクトリ / LBA4: CONFIG.HTM の中身
// ------------------------------------------------------------------

// CONFIG.HTM の中身（1セクタ=512バイト未満であること）
static const char INDEX_HTML_CONTENT[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<title>Setup</title></head><body style=\"text-align:center;padding-top:50px;\">"
    "<h2>RP2040 Change settings</h2>"
    "<h2>マクロ設定ページ↓</h2>"
    "<a href=\"https://katolabmain.github.io/shortcut_keyboard/\" style=\"font-size:20px;\">ここをクリック(Open Page)</a>"
    "</body></html>";
static_assert(sizeof(INDEX_HTML_CONTENT) - 1 < 512, "CONFIG.HTM は1セクタ(512B)に収めること");

static const char README_TXT_CONTENT[] =
    "お買い上げありがとうございます。\n"
    "このキーボードは, RP2040マイコンを搭載し, HID(キーボード+メディアキー) / MSC(設定用USBメモリ) / CDC(Serial) の3つの機能を持つUSB複合デバイスです。\n"
    "設定ページは CONFIG.HTMを開くか(https://katolabmain.github.io/shortcut_keyboard/) からアクセスしてください。\n"
    "内部のドキュメントは(https://github.com/katolabmain/shortcut_keyboard) から参照してください。\n";
static_assert(sizeof(README_TXT_CONTENT) - 1 < 512, "README.TXT は1セクタ(512B)に収めること");

static const char README_EN_TXT_CONTENT[] =
    "Thank you for purchasing.\n"
    "This RP2040-powered USB composite device features HID (Keyboard + Media Keys), MSC (Mass Storage for settings), and CDC (Serial) interfaces.\n"
    "To access the settings page, open CONFIG.HTM or visit (https://katolabmain.github.io/shortcut_keyboard/).\n"
    "For internal documentation, please refer to (https://github.com/katolabmain/shortcut_keyboard).\n";
static_assert(sizeof(README_EN_TXT_CONTENT) - 1 < 512, "READMEEN.TXT は1セクタ(512B)に収めること");

static int32_t msc_read_cb(uint32_t lba, void *buffer, uint32_t bufsize)
{
    memset(buffer, 0, bufsize);

    if (lba == 0)
    {
        uint8_t *boot = (uint8_t *)buffer;
        boot[0] = 0xEB;
        boot[1] = 0x3C;
        boot[2] = 0x90;
        memcpy(&boot[3], "MSDOS5.0", 8);
        boot[11] = 0x00; // 1セクタのバイト数 = 0x0200 (512)
        boot[12] = 0x02;
        boot[13] = 1; // 1クラスタのセクタ数
        boot[14] = 1; // 予約セクタ数
        boot[15] = 0;
        boot[16] = 2;  // FAT の数
        boot[17] = 16; // ルートディレクトリのエントリ数
        boot[18] = 0;
        boot[19] = 16; // 総セクタ数
        boot[20] = 0;
        boot[21] = 0xF8; // メディア記述子
        boot[22] = 1;    // 1つのFATのセクタ数
        boot[23] = 0;
        boot[36] = 0x00; // ドライブ番号
        boot[37] = 0x00; // 予約
        boot[38] = 0x29; // 拡張ブートシグネチャ (必須)
        boot[39] = 0x12;
        boot[40] = 0x34; // シリアル番号 (任意)
        boot[41] = 0x56;
        boot[42] = 0x78;

        // --- ボリュームラベル (11文字固定) ---
        memcpy(&boot[43], "RP2040 DISK", 11);

        // --- ファイルシステムタイプ (8文字固定) ---
        memcpy(&boot[54], "FAT12   ", 8);
        boot[510] = 0x55;
        boot[511] = 0xAA;
    }
    else if (lba == 1 || lba == 2)
    {
        // FAT: クラスタ0,1(予約) + クラスタ2(CONFIG.HTM) + クラスタ3(README.TXT) = 4クラスタ分のFATを作る
        uint8_t *fat = (uint8_t *)buffer;
        fat[0] = 0xF8;
        fat[1] = 0xFF;
        fat[2] = 0xFF;
        fat[3] = 0xFF;
        fat[4] = 0xFF;
        fat[5] = 0xFF;
        fat[6] = 0xFF;
        fat[7] = 0x0F;
    }
    else if (lba == 3)
    {
        uint8_t *entry = (uint8_t *)buffer;

        // 【エントリ1】ボリュームラベル (0〜31バイト目)
        memcpy(&entry[0], "RP2040 DISK", 11); // ラベル名 (11文字)
        entry[11] = 0x08;                     // 属性: 0x08 (Volume Label)

        // 【エントリ2】CONFIG.HTM (32〜63バイト目)
        memcpy(&entry[32], "CONFIG  HTM", 11); // ファイル名8文字 + 拡張子3文字
        entry[32 + 11] = 0x21;                 // 属性: 0x20 (Archive) + 0x01 (Read Only)
        entry[32 + 26] = 2;                    // 開始クラスタ番号: 2 (LBA 4を指す)
        entry[32 + 27] = 0;
        uint32_t fileSize_CONFIG_HTML = sizeof(INDEX_HTML_CONTENT) - 1; // ファイルサイズ
        memcpy(&entry[32 + 28], &fileSize_CONFIG_HTML, sizeof(fileSize_CONFIG_HTML));
        //  【エントリ3】README.TXT (64〜95バイト目)
        memcpy(&entry[64], "READMEjpTXT", 11); // ファイル名8文字 + 拡張子3文字
        entry[64 + 11] = 0x21;                 // 属性: 0x20 (Archive) + 0x01 (Read Only)
        entry[64 + 26] = 3;                    // 開始クラスタ番号: 3 (LBA 5を指す)
        entry[64 + 27] = 0;
        uint32_t fileSize_README_TXT = sizeof(README_TXT_CONTENT) - 1; // ファイルサイズ
        memcpy(&entry[64 + 28], &fileSize_README_TXT, sizeof(fileSize_README_TXT));
        //  【エントリ4】READMEEN.TXT (96〜127バイト目)
        memcpy(&entry[96], "READMEenTXT", 11); // ファイル名8文字 + 拡張子3文字
        entry[96 + 11] = 0x21;                 // 属性: 0x20 (Archive) + 0x01 (Read Only)
        entry[96 + 26] = 4;                    // 開始クラスタ番号: 4 (LBA 6を指す)
        entry[96 + 27] = 0;
        uint32_t fileSize_READMEEN_TXT = sizeof(README_EN_TXT_CONTENT) - 1; // ファイルサイズ
        memcpy(&entry[96 + 28], &fileSize_READMEEN_TXT, sizeof(fileSize_READMEEN_TXT));
    }
    else if (lba == 4)
    {
        size_t content_len = sizeof(INDEX_HTML_CONTENT) - 1;
        memcpy(buffer, INDEX_HTML_CONTENT, content_len);
    }
    else if (lba == 5)
    {
        size_t content_len = sizeof(README_TXT_CONTENT) - 1;
        memcpy(buffer, README_TXT_CONTENT, content_len);
    }
    else if (lba == 6)
    {
        size_t content_len = sizeof(README_EN_TXT_CONTENT) - 1;
        memcpy(buffer, README_EN_TXT_CONTENT, content_len);
    }

    return (int32_t)bufsize;
}

static void msc_flush_cb(void) {}

// ------------------------------------------------------------------
// 公開関数
// ------------------------------------------------------------------
void usbDetach()
{
    // 【最重要】PCへのUSB検出を一度強制的に切断・遮断する
    TinyUSBDevice.detach();
    delay(100);
}

void usbBegin()
{
    // 古いキャッシュを回避するため複合デバイス用VID/PIDを確実に指定
    TinyUSBDevice.setID(0x239A, 0x8014);
    TinyUSBDevice.setManufacturerDescriptor("Raspberry Pi");
    TinyUSBDevice.setProductDescriptor("Pico W Composite Device");

    // 1. HID インターフェース設定
    usb_hid.setPollInterval(2);
    usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    usb_hid.begin();

    // 2. MSC インターフェース設定
    usb_msc.setID("RaspberryPi", "USB Drive", "1.0");
    usb_msc.setCapacity(16, 512);
    usb_msc.setReadWriteCallback(msc_read_cb, NULL, msc_flush_cb);
    usb_msc.setUnitReady(true);
    usb_msc.begin();
    usb_msc.setReadyCallback([]()
                             { msc_ready = true; return true; });

    // 3. CDC シリアル設定
    Serial.begin(115200);

    // 構築完了後、少し待ってから一括でPCへ接続通知（アタッチ）を出す
    delay(300);
    TinyUSBDevice.attach();
}

void usbWaitReady()
{
    // いずれかが true になった時点で抜ける (全て false の間だけ待つ)
    while (!TinyUSBDevice.ready() && !usb_hid.ready() && !tud_cdc_connected() && !tud_hid_ready() && !Serial.dtr() && !msc_ready)
    {
        tud_task();
        delay(10);
    }
}

void usbPump(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        tud_task();
        delay(1);
    }
}
