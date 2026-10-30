#pragma once
#include <stddef.h>
#include <stdint.h>

// マクロ設定(364バイト)を EEPROM(フラッシュ末尾のエミュレーション領域)へ保存/読込する
//
// EEPROM上のレイアウト:
//   [0..363]   設定データ (先頭4バイト=タイムスタンプ + 36バイト x 10スイッチ)
//   [364..365] マジック 0x4D43 (リトルエンディアン)
//   [366..367] 設定データの CRC16-CCITT (リトルエンディアン)
// 以前の書き込み内容(別ファームの残骸など)を誤って設定として読まないよう、
// マジックとCRCが一致したときだけ有効とみなす。
namespace ConfigStorage
{
    constexpr size_t CONFIG_SIZE = 364;

    // EEPROMを初期化する (起動時に1回)
    void begin();

    // 読み出す。有効なら true。無効なら out を 0xFF で埋めて false を返す
    // (MacroManager::loadConfig は先頭が 0xFFFFFFFF だと内蔵の既定値を使う)
    bool load(uint8_t out[CONFIG_SIZE]);

    // 書き込んで commit する。成功すれば true
    bool save(const uint8_t in[CONFIG_SIZE]);

    // 保存領域を消去する (次回起動時は内蔵の既定値になる)
    bool erase();
}
