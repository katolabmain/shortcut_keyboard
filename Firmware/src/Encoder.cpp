#include "Encoder.h"
#include "App.h"
#include "pin.h"

// ------------------------------------------------------------------
// 割り込みで更新される値 (core0 の ISR が書き込み、loop() が読む)
// ------------------------------------------------------------------
static volatile int enc1_count = 0;
static volatile int enc2_count = 0;
static volatile uint8_t enc1_state = 0;
static volatile uint8_t enc2_state = 0;

static volatile uint32_t enc1_set_time = 0;
static volatile uint32_t enc2_set_time = 0;

// 直前2bit + 現在2bit のA/B相の組み合わせ → -1/0/+1
static const int8_t KNOB_TABLE[] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0};

// 4カウントで1クリック分。端数(4未満)を持ち越すための値
static int enc1_data = 0;
static int enc2_data = 0;

// ------------------------------------------------------------------
// 割り込みハンドラ
// ------------------------------------------------------------------
static void handleEncoder1()
{
    // gpio_get() により 1〜2 クロックで状態取得
    uint8_t a = gpio_get(PIN::ENC1_PIN_A);
    uint8_t b = gpio_get(PIN::ENC1_PIN_B);

    enc1_state = ((enc1_state << 2) | (a << 1) | b) & 0x0F;
    enc1_count += KNOB_TABLE[enc1_state];
}

static void handleEncoder2()
{
    uint8_t a = gpio_get(PIN::ENC2_PIN_A);
    uint8_t b = gpio_get(PIN::ENC2_PIN_B);

    enc2_state = ((enc2_state << 2) | (a << 1) | b) & 0x0F;
    enc2_count += KNOB_TABLE[enc2_state];
}

// ------------------------------------------------------------------
// 公開関数
// ------------------------------------------------------------------
void encoderInit()
{
    pinMode(PIN::ENC1_PIN_A, INPUT_PULLUP);
    pinMode(PIN::ENC1_PIN_B, INPUT_PULLUP);
    pinMode(PIN::ENC2_PIN_A, INPUT_PULLUP);
    pinMode(PIN::ENC2_PIN_B, INPUT_PULLUP);

    enc1_state = (gpio_get(PIN::ENC1_PIN_A) << 1) | gpio_get(PIN::ENC1_PIN_B);
    enc2_state = (gpio_get(PIN::ENC2_PIN_A) << 1) | gpio_get(PIN::ENC2_PIN_B);
}

void encoderAttachInterrupts()
{
    attachInterrupt(digitalPinToInterrupt(PIN::ENC1_PIN_A), handleEncoder1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN::ENC1_PIN_B), handleEncoder1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN::ENC2_PIN_A), handleEncoder2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN::ENC2_PIN_B), handleEncoder2, CHANGE);
}

void encoderPoll()
{
    noInterrupts();
    int rawEnc1 = enc1_count;
    int rawEnc2 = enc2_count;
    // 4の倍の分解能でカウントされるので、4で割った余りだけ残すことでオーバーフローを防ぐ
    enc1_count = enc1_count & 0x03; // %4を高速化する為、ビット演算を使用
    enc2_count = enc2_count & 0x03;
    interrupts();

    // 4で割った商(負数は算術シフトで切り下げ)を積算。余りは上で enc_count に残している
    enc1_data += rawEnc1 >> 2;
    enc2_data += rawEnc2 >> 2;

    if (enc1_data != 0)
    {
        if (enc1_data > 0)
        {
            enc1_data--;
            macroManager.triggerMacro(8); // 時計回り (既定: ボリュームアップ)
        }
        else
        {
            enc1_data++;
            macroManager.triggerMacro(9); // 反時計回り (既定: ボリュームダウン)
        }
        enc1_set_time = millis();
    }
    if (enc2_data != 0)
    {
        if (enc2_data > 0)
        {
            enc2_data--;
            macroManager.triggerMacro(6); // 時計回り (既定: 明るさアップ)
        }
        else
        {
            enc2_data++;
            macroManager.triggerMacro(7); // 反時計回り (既定: 明るさダウン)
        }
        enc2_set_time = millis();
    }
}

uint32_t encoderLastRotateMs(int idx)
{
    return (idx == 0) ? enc1_set_time : enc2_set_time;
}
