#include "KeyInput.h"
#include "App.h"
#include "pin.h"
#include "sw_control.h"

// 検出するスイッチ (この並びが keynumbers[] と対応)
static sw_control buttons[]{
    sw_control(PIN::sw1),
    sw_control(PIN::sw2),
    sw_control(PIN::sw3),
    sw_control(PIN::sw4),
    sw_control(PIN::ENC1_SW_PIN),
    sw_control(PIN::ENC2_SW_PIN)};

// buttons[i] が押されたときに起動するマクロ番号
//   0:右上 1:右下 2:左上 3:左下 4:上ダイヤル押し込み 5:下ダイヤル押し込み
//   (6〜9 はエンコーダー回転用。Encoder.cpp 参照)
static const uint8_t keynumbers[] = {3, 2, 1, 0, 5, 4};

void inputInit()
{
    pinMode(PIN::sw1, INPUT_PULLUP);
    pinMode(PIN::sw2, INPUT_PULLUP);
    pinMode(PIN::sw3, INPUT_PULLUP);
    pinMode(PIN::sw4, INPUT_PULLUP);

    pinMode(PIN::ENC1_SW_PIN, INPUT_PULLUP);
    pinMode(PIN::ENC2_SW_PIN, INPUT_PULLUP);
}

void inputPoll()
{
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++)
    {
        if (buttons[i].sw_check())
        {
            macroManager.triggerMacro(keynumbers[i]);
        }
    }
}
