#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "App.h"
#include "ConfigStorage.h"
#include "Encoder.h"
#include "KeyInput.h"
#include "SerialConsole.h"
#include "StatusLed.h"
#include "UsbSetup.h"

// ------------------------------------------------------------------
// ファイル構成
//   main.cpp           : setup/loop の流れだけ
//   UsbSetup.cpp       : USB複合デバイス(HID/MSC/CDC)の設定
//   ConfigStorage.cpp  : マクロ設定の EEPROM 保存/読込
//   SerialConsole.cpp  : Serial からの設定確認・編集コマンド
//   Encoder.cpp        : ロータリーエンコーダー(割り込み)
//   KeyInput.cpp       : 押しボタンの検出
//   StatusLed.cpp      : LED表示 (core1)
//   lib/MacroManager   : マクロの実行エンジン
// ------------------------------------------------------------------

MacroManager macroManager;
volatile bool isConnected = false;

// ------------------------------------------------------------------
// core0: USB / マクロ実行 / 入力
// ------------------------------------------------------------------
void setup()
{
    // PCへのUSB検出を一度切断してから、構成が整ってから接続し直す
    usbDetach();

    // EEPROM から設定を読み込む (無効・未書き込みなら内蔵の既定値)
    ConfigStorage::begin();
    uint8_t configData[MacroManager::TOTAL_CONFIG_SIZE];
    ConfigStorage::load(configData); // 無効時は全て 0xFF になり、loadConfig が既定値を使う
    bool fromEeprom = macroManager.loadConfig(configData);
    macroManager.init(&usb_hid);
    consoleBegin(macroManager, fromEeprom);

    usbBegin();

    // GPIO の設定
    encoderInit();
    inputInit();

    // USB デバイスの準備が整うまで待機
    usbWaitReady();
    encoderAttachInterrupts();
    usbPump(1000); // USB 列挙が落ち着くまで待つ

    isConnected = true;
}

void loop()
{
    tud_task();                 // TinyUSB デバイスのタスク
    macroManager.processTask(); // HID キー送信タスク
    consolePoll();              // Serial 接続通知とコマンド処理
    encoderPoll();              // ダイヤル回転 → マクロ
    inputPoll();                // ボタン押下 → マクロ
}

// ------------------------------------------------------------------
// core1: LED
// ------------------------------------------------------------------
void setup1()
{
    ledSetup();
}

void loop1()
{
    ledLoop();
}
