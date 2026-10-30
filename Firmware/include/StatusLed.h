#pragma once

// LED表示 (core1 で動かす: setup1()/loop1() から呼ぶ)
//   - NeoPixel 4個: 押しボタンごとの色表示 (ColorSwitch)
//   - ダイヤル周りのLED: 呼吸するように明滅。回した直後は速く、押し込み中は点灯側が切り替わる

// LEDピンの設定とNeoPixelの初期化
void ledSetup();

// USB接続待ち中は白点滅、接続後は通常のLED表示を1周ぶん行う
void ledLoop();
