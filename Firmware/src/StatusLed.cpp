#include "StatusLed.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "App.h"
#include "Encoder.h"
#include "pin.h"
#include "ColorSwitch.h"

// ------------------------------------------------------------------
// インスタンス
// ------------------------------------------------------------------
Adafruit_NeoPixel pixels(PIN::NUM_LEDS, PIN::RGBLED_PIN, NEO_GRB + NEO_KHZ800);

// 押しボタンごとのLED (第2引数は NeoPixel 上のLED番号)
static ColorSwitch sw[]{
    ColorSwitch(PIN::sw1, 1),
    ColorSwitch(PIN::sw2, 2),
    ColorSwitch(PIN::sw3, 0),
    ColorSwitch(PIN::sw4, 3)};

// ダイヤルLEDの呼吸(正弦波)の位相。[0]=ENC1, [1]=ENC2
static float cos_phase[2] = {0.0, 0.0};
// 位相を最後に更新した時刻 [ms]
// (以前は float だったため、約4.6時間以降は millis() を正確に保持できず位相が乱れた)
static uint32_t last_time[2] = {0, 0};

// ------------------------------------------------------------------
// 内部関数
// ------------------------------------------------------------------
static void sw_update()
{
    for (size_t i = 0; i < sizeof(sw) / sizeof(sw[0]); i++)
    {
        sw[i].update();
    }
}

// 位相を進める。回した直後(1秒以内)は速く、それ以外はゆっくり
//   ゆっくり: 位相が π/500 rad/ms → 1周期 1秒
//   速い    : 位相が π/100 rad/ms → 1周期 0.2秒
static void advancePhase(int idx)
{
    uint32_t now = millis();
    uint32_t dt = now - last_time[idx];
    if (now - encoderLastRotateMs(idx) > 1000)
    {
        cos_phase[idx] += dt * PI / 500.0;
    }
    else
    {
        cos_phase[idx] += dt * PI / 100.0;
    }
    last_time[idx] = now;
    cos_phase[idx] = fmod(cos_phase[idx], 2 * PI);
}

// ダイヤルの2色LEDを駆動する。押し込んでいない時は led1、押している時は led2 が呼吸する
static void driveKnobLed(int led1, int led2, int swPin, float phase)
{
    int brightness = 100 * (1 + cosf(phase)) / 2;
    if (gpio_get(swPin))
    {
        analogWrite(led1, brightness);
        analogWrite(led2, 0);
    }
    else
    {
        analogWrite(led1, 0);
        analogWrite(led2, brightness);
    }
}

// 2つのダイヤルの位相がずれている場合、ゆっくり一致するように合わせていく
// (10msごとに0.01radずつ)
static void syncPhases()
{
    static unsigned long last_adjust_time = 0;
    if (millis() - last_adjust_time < 10)
    {
        return;
    }
    last_adjust_time = millis();
    float phase_diff = cos_phase[0] - cos_phase[1];
    if (fabs(phase_diff) > 0.01)
    {
        if (phase_diff > 0)
        {
            cos_phase[1] += 0.01;
        }
        else
        {
            cos_phase[0] += 0.01;
        }
    }
    else
    {
        cos_phase[0] = cos_phase[1];
    }
}

// ------------------------------------------------------------------
// 公開関数
// ------------------------------------------------------------------
void ledSetup()
{
    pinMode(PIN::ENC1_LED1, OUTPUT);
    pinMode(PIN::ENC1_LED2, OUTPUT);
    pinMode(PIN::ENC2_LED1, OUTPUT);
    pinMode(PIN::ENC2_LED2, OUTPUT);

    analogWrite(PIN::ENC1_LED1, 0);
    analogWrite(PIN::ENC1_LED2, 0);
    analogWrite(PIN::ENC2_LED1, 0);
    analogWrite(PIN::ENC2_LED2, 0);

    pixels.begin();
    pixels.clear();
    pixels.show();
}

void ledLoop()
{
    // LED 準備中モーション 白点滅x2+少し待つ
    if (!isConnected)
    {
        for (int i = 0; i < 2; i++)
        {
            pixels.fill(pixels.Color(30, 30, 30));
            pixels.show();
            delay(100);
            pixels.clear();
            pixels.show();
            delay(100);
        }
        delay(200);
        return;
    }

    sw_update();
    pixels.show();

    advancePhase(0);
    advancePhase(1);

    driveKnobLed(PIN::ENC1_LED1, PIN::ENC1_LED2, PIN::ENC1_SW_PIN, cos_phase[0]);
    driveKnobLed(PIN::ENC2_LED1, PIN::ENC2_LED2, PIN::ENC2_SW_PIN, cos_phase[1]);

    syncPhases();
}
