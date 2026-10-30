#pragma once
#include "MacroManager.h"

// Serial(USB CDC)から マクロ設定を確認・編集・EEPROM保存するための簡易コンソール
// コマンド一覧は起動後に "help" で表示 (詳細は docs/EEPROM_SERIAL_GUIDE.md)

// mm: 編集対象 / configFromEeprom: 起動時に EEPROM から読めたか (false なら内蔵の既定値)
void consoleBegin(MacroManager &mm, bool configFromEeprom);

// loop() から毎回呼ぶ。Serial接続時の案内表示と、入力1行ごとのコマンド実行を行う
void consolePoll();
