#pragma once

// 押しボタン(4個) + ダイヤル押し込み(2個) の入力処理

// GPIOをプルアップ入力に設定する
void inputInit();

// loop() から毎回呼ぶ。押された瞬間に対応するマクロを起動する
void inputPoll();
