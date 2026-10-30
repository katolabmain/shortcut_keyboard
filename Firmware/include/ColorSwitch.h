#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// LED制御用のNeoPixel(実体は StatusLed.cpp)
extern Adafruit_NeoPixel pixels;

static const uint32_t LED_COLOR_map[]{
    pixels.Color(255, 0, 0),   // 赤
    pixels.Color(0, 255, 0),   // 緑
    pixels.Color(0, 0, 255),   // 青
    pixels.Color(255, 255, 0), // 黄
    pixels.Color(0, 255, 255), // シアン
    pixels.Color(255, 0, 255), // マゼンタ
};
class ColorSwitch
{
private:
    uint8_t pin;                  // スイッチのピン番号
    uint8_t ledIndex;             // 制御するLEDのインデックス
    bool pushed;                  // 現在のスイッチ押下状態
    uint32_t lastChangeTime;      // 状態変化時のタイムスタンプ
    uint8_t lastColorIndex;       // 1個前の色インデックス
    uint8_t secondLastColorIndex; // 2個前の色インデックス
    uint32_t lastDimTime;         // 消灯をゆっくりにするためのタイムスタンプ
    bool dimming;                 // 消灯フェード中かどうか
    uint8_t colorIndex;           // 現在の色インデックス
    // 0〜5 の偏りのない完全均等な乱数を返す関数
    uint8_t getRand6()
    {
        // 6の倍数になる上限(2^32 を 6 で割り切れる範囲)。r がこれ以上なら端数なので捨てる
        const uint32_t limit = (UINT32_MAX / 6) * 6;

        uint32_t r;
        do
        {
            r = rp2040.hwrand32(); // 0〜4294967295 の乱数を取得
        } while (r >= limit); // 端数が出たら捨てて引き直す

        return r % 6;
    }

    // 重複（1個前・2個前）を避けて新しい色インデックスを取得する非公開メソッド
    uint8_t getNextColorIndex()
    {
        uint8_t newColorIndex;
        while (true)
        {
            newColorIndex = getRand6();
            if (newColorIndex != lastColorIndex && newColorIndex != secondLastColorIndex)
            {
                secondLastColorIndex = lastColorIndex;
                lastColorIndex = newColorIndex;
                return newColorIndex;
            }
        }
    }

public:
    // コンストラクタ
    ColorSwitch(uint8_t swPin, uint8_t ledIdx = 0)
        : pin(swPin),
          ledIndex(ledIdx),
          pushed(false),
          lastChangeTime(0),
          lastColorIndex(255),
          secondLastColorIndex(255),
          lastDimTime(0),
          dimming(false),
          colorIndex(255) {}

    // 更新処理（loop内で常時呼び出す）
    void update()
    {
        bool currentPinState = !gpio_get(pin);

        // チャタリング防止（状態変化 ＆ 10ms経過）
        if (currentPinState != pushed && (millis() - lastChangeTime > 10))
        {
            lastChangeTime = millis();
            pushed = currentPinState;

            if (pushed)
            {
                // 押された時: 重複しない新色をセット
                colorIndex = getNextColorIndex();
                dimming = false;
                pixels.setPixelColor(ledIndex, LED_COLOR_map[colorIndex]);
            }
            else
            {
                // 離された時: 消灯
                // ゆっくり消灯するためのタイムスタンプを記録
                lastDimTime = millis();
                dimming = true;
            }
        }
        // 離された後,300msにかけて徐々に消灯する
        // (dimming は一度でも押された後にだけ true になるので colorIndex は必ず有効値)
        if (!pushed && dimming)
        {
            uint32_t elapsed = millis() - lastDimTime;
            if (elapsed >= 300)
            {
                // フェード完了: 確実に消灯して終了
                pixels.setPixelColor(ledIndex, 0);
                dimming = false;
                return;
            }
            // 徐々に消灯するための係数を計算
            // 0ms -> 1.0, 300ms -> 0.0
            float dimFactor = 1.0f - static_cast<float>(elapsed) / 300.0f;
            uint32_t currentColor = LED_COLOR_map[colorIndex];
            uint8_t r = (currentColor >> 16) & 0xFF;
            uint8_t g = (currentColor >> 8) & 0xFF;
            uint8_t b = currentColor & 0xFF;

            // 色を徐々に暗くする
            r = static_cast<uint8_t>(r * dimFactor);
            g = static_cast<uint8_t>(g * dimFactor);
            b = static_cast<uint8_t>(b * dimFactor);

            pixels.setPixelColor(ledIndex, r, g, b);
        }
    }

    // 押されているかどうかを取得するゲッター
    bool isPushed() const
    {
        return pushed;
    }
};