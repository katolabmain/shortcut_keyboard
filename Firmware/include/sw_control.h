#pragma once
#include <Arduino.h>
class sw_control
{
private:
    uint8_t pin;             // スイッチのピン番号
    bool pushed;             // 現在のスイッチ押下状態
    uint32_t lastChangeTime; // 状態変化時のタイムスタンプ

public:
    sw_control(uint8_t swPin)
        : pin(swPin),
          pushed(false),
          lastChangeTime(0)
    {
    }
    bool sw_check()
    {
        bool currentPinState = !gpio_get(pin);

        // チャタリング防止（状態変化 ＆ 10ms経過）
        if (currentPinState != pushed && (millis() - lastChangeTime > 10))
        {
            lastChangeTime = millis();
            pushed = currentPinState;

            if (pushed)
            {
                return true;
            }
        }
        return false;
    }
};