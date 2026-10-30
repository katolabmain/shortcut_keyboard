#pragma once
#include <Arduino.h>

// ロータリーエンコーダー2個の読み取り
//   ENC1 = 下ダイヤル: 時計回り=マクロ8 / 反時計回り=マクロ9 (既定: 音量 +/-)
//   ENC2 = 上ダイヤル: 時計回り=マクロ6 / 反時計回り=マクロ7 (既定: 明るさ +/-)

// GPIOの設定と初期状態の取得 (割り込み登録の前に呼ぶ)
void encoderInit();

// A/B相の変化割り込みを登録する
void encoderAttachInterrupts();

// loop() から毎回呼ぶ。回転量をマクロ起動に変換する
void encoderPoll();

// idx(0:ENC1, 1:ENC2) が最後に回された時刻 [ms]。LEDの点滅速度の切り替えに使う
uint32_t encoderLastRotateMs(int idx);
