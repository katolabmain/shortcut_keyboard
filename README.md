# RP2040 USB マクロキーボード

Raspberry Pi Pico w(RP2040)で動く、**ボタン 4 個 + ロータリーエンコーダー 2 個**のマクロパッドの設計データおよびファームウェアです。
ボタン・ダイヤル押し込み・ダイヤル回転(左右)の計 **10 個の入力**それぞれに、最大 16 ステップのキー操作(マクロ)を割り当てられます。

- マクロはブラウザの設定ツール、または USB Serial コマンドで変更できます(ファーム書き換え不要)
- 設定は本体のフラッシュ(EEPROM エミュレーション領域)に保存され、ファームを更新しても残ります
- PC からは「キーボード + メディアキー」「設定用 USB メモリ」「シリアルポート」の 3 つを持つ USB 複合デバイスとして認識されます

## 特徴

| 項目 | 内容 |
|---|---|
| マクロ数 | 10(ボタン 4 + ダイヤル押し込み 2 + ダイヤル回転 4 方向) |
| 1 マクロの長さ | 最大 16 ステップ |
| ステップの種類 | キー押下 / キー解除 / 打鍵 / 全キー解除 / ウェイト(1〜65535 ms) |
| メディアキー | 音量・再生・画面の明るさなど(Consumer Control) |
| 設定の保存 | EEPROM(マジック + CRC16 で検証。無効なら内蔵の既定値で動作) |
| 設定方法 | ブラウザ設定ツール(`index.html` / Web Serial対応) / Serial コマンド |
| LED | NeoPixel 4 個(押すとランダム色)+ ダイヤル周りの呼吸 LED |

## 既定のマクロ

| sw | 入力 | 既定の動作 |
|---|---|---|
| 0 | 右上ボタン | Ctrl+V |
| 1 | 右下ボタン | Ctrl+S |
| 2 | 左上ボタン | Ctrl+C |
| 3 | 左下ボタン | F4 |
| 4 | 上ダイヤル押し込み | F11 |
| 5 | 下ダイヤル押し込み | Ctrl+Shift+M |
| 6 | 上ダイヤル 時計回り | 画面の明るさアップ |
| 7 | 上ダイヤル 反時計回り | 画面の明るさダウン |
| 8 | 下ダイヤル 時計回り | 音量アップ |
| 9 | 下ダイヤル 反時計回り | 音量ダウン |

既定値は `Firmware/lib/MacroManager/macro_config.h` に内蔵されています。EEPROM に有効な設定がないときだけ使われます。

## 使い方

### 1. ファームウェアを書き込む

0. 事前にファームウェアが書き込まれていますが、何らかのトラブルで内部のファームウェアが破損することもあります。
1. USBを接続する
2. BOOT ボタンを押したままにする
3. RUN ボタンを押して離す
4. 最後に BOOT ボタンを離す
5. `RPI-RP2` ドライブとして見えます。
6. `Firmware/build/RP2040_keyboard.uf2` をそのドライブへコピーします。
7. 自動で再起動し、キーボードとして動作します。

> 通常の UF2 書き込みでは EEPROM 領域は消えないため、保存済みの設定はそのまま残ります。

### 2. マクロを設定する

接続すると `RP2040 DISK` という小さな USB ドライブが見えます。

| ファイル | 内容 |
|---|---|
| `CONFIG.HTM` | 設定ページへのリンク(ダブルクリックで開く) |
| `README.TXT` / `READMEEN.TXT` | 簡単な案内(日本語 / 英語) |

設定ページ: <https://katolabmain.github.io/shortcut_keyboard/>

設定ツールは `index.html` と同じものです。サーバーやインストールは不要で、ブラウザで開くだけで動きます(オフライン可)。

1. 「**COMポート選択**」を押し、接続するマクロパッドを選択する（本体内の設定が自動読み込みされます）
2. 10 個のスイッチ列に、Step #1〜#16 でマクロを組む（変更内容はリアルタイムで本体に送信されます）
3. 設定が終わったら「**本体へ保存 (save)**」を押して EEPROM に保存する

そのほか、バックアップ用として設定を `.bin`(364 バイト)で保存・読み込みができます。
画面の設定は自動保存されないため、オフライン作業時などは `.bin` を保存してください。
詳細は [Firmware/docs/INDEX_HTML_GUIDE.md](/Firmware/docs/INDEX_HTML_GUIDE.md) を参照してください。

### 3. Serial から設定する(上級者向け)

115200 bps のシリアルターミナル(`pio device monitor`、Tera Term、PuTTY など)で接続し、`help` を入力するとコマンド一覧が出ます。

```
show 0                 # sw0 のマクロを表示
set 0 1 down E0        # sw0 の 1 ステップ目: 左 Ctrl を押す
set 0 2 down E1        # 2 ステップ目: 左 Shift を押す
set 0 3 tap 29         # 3 ステップ目: Esc を打鍵
set 0 4 rel            # 4 ステップ目: 全キー解除
save                   # EEPROM に保存
```

| コマンド | 内容 |
|---|---|
| `status` / `show [sw]` / `dump` | 状態・内容の確認、設定全体の 16 進数出力 |
| `set <sw> <step> down\|up\|tap <hex>` | キー操作ステップの設定 |
| `set <sw> <step> rel` / `wait <ms>` / `clear` | 全解除 / ウェイト / ステップ削除 |
| `load <728桁の16進数>` | 設定全体の置き換え |
| `time <UNIX秒>` | 設定のタイムスタンプ変更 |
| `save` / `reload` / `erase yes` | EEPROM へ保存 / 読み直し / 消去して既定値へ |
| `echo on\|off` | エコーバックの切り替え |

`set` / `load` / `time` は RAM 上の設定を即座に書き換えるだけで、**`save` するまで EEPROM には反映されません**(電源を切ると元に戻ります)。
詳細・HID コード表・トラブルシュートは [/Firmware/docs/EEPROM_SERIAL_GUIDE.md](/Firmware/docs/EEPROM_SERIAL_GUIDE.md) を参照してください。

## USB 構成

| 項目 | 内容 |
|---|---|
| VID / PID | `0x239A` / `0x8014` |
| 製造元 / 製品名 | `Raspberry Pi` / `Pico W Composite Device` |
| HID | キーボード(Report ID 1)+ Consumer Control(Report ID 2)、ポーリング間隔 2 ms |
| MSC | 8 KB(16 セクタ)の読み取り専用 FAT12 ボリューム `RP2040 DISK` |
| CDC | Serial(115200 bps) |

## ハードウェア(GPIO 割り当て)

`include/pin.h` で定義しています。すべてプルアップ入力です。
押すと LOWになります。

| 機能 | GPIO |
|---|---|
| NeoPixel データ(4 個) | 17 |
| 押しボタン sw1 / sw2 / sw3 / sw4 | 8 / 9 / 10 / 11 |
| ダイヤル 1(下)A 相 / B 相 / 押し込み | 18 / 19 / 5 |
| ダイヤル 2(上)A 相 / B 相 / 押し込み | 20 / 21 / 4 |
| ダイヤル 1 の LED(通常 / 押し込み中) | 26 / 27 |
| ダイヤル 2 の LED(通常 / 押し込み中) | 28 / 29 |

ボタンとマクロ番号の対応(`src/KeyInput.cpp`)は、sw1 → マクロ 3(左下)、sw2 → 2(左上)、sw3 → 1(右下)、sw4 → 0(右上)です。

## LED の動き

- **USB 接続待ち**: NeoPixel が白く 2 回点滅 → 少し待つ、を繰り返します。
- **ボタン**: 押すと 6 色(赤・緑・青・黄・シアン・マゼンタ)からランダムに点灯します。直前 2 回と同じ色は出ません。離すと 300 ms かけて消灯します。
- **ダイヤル**: 呼吸するように明滅します(周期 1 秒)。回した直後の 1 秒間は速く(周期 0.2 秒)、押し込み中は点灯する LED が切り替わります。2 つのダイヤルの明滅位相は自動で揃います。

## ビルド

PlatformIO でビルドします(設定は `platformio.ini`)。

| 項目 | 内容 |
|---|---|
| ボード | `rpipicow`(Raspberry Pi Pico W) |
| フレームワーク | Arduino(earlephilhower コア / [maxgerhardt/platform-raspberrypi](https://github.com/maxgerhardt/platform-raspberrypi)) |
| ライブラリ | Adafruit TinyUSB Library `^3.1.0`、Adafruit NeoPixel `^1.12.0` |
| ビルドフラグ | `-DUSE_TINYUSB`(USB スタックに TinyUSB を使う) |
| モニタ速度 | 115200 bps |

```bash
pio run              # ビルド
pio run -t upload    # 書き込み
pio device monitor   # Serial モニタ(115200 bps)
```

## ファイル構成

```
.
├── LICENSE                            ライセンス
├── README.md                          全体の説明書
├── index.html                         マクロ設定ツール
├── Firmware/
│   ├── platformio.ini                 PlatformIO 設定ファイル
│   ├── build/
│   │   └── RP2040_keyboard.uf2        ビルド済みファームウェア
│   ├── docs/
│   │   ├── EEPROM_SERIAL_GUIDE.md     EEPROM 保存と Serial コマンドの詳細
│   │   └── INDEX_HTML_GUIDE.md        ブラウザ設定ツールの使い方
│   ├── include/
│   │   ├── App.h                      アプリ共有オブジェクト
│   │   ├── ColorSwitch.h              ボタンごとの LED 色制御
│   │   ├── ConfigStorage.h            EEPROM 保存/読込（ヘッダー）
│   │   ├── Encoder.h                  エンコーダー処理（ヘッダー）
│   │   ├── KeyInput.h                 ボタン入力処理（ヘッダー）
│   │   ├── pin.h                      GPIO 割り当て定義
│   │   ├── SerialConsole.h            Serial コンソール（ヘッダー）
│   │   ├── StatusLed.h                LED 表示（ヘッダー）
│   │   ├── sw_control.h               ボタンのチャタリング除去
│   │   └── UsbSetup.h                 USB 複合デバイス設定（ヘッダー）
│   ├── lib/
│   │   └── MacroManager/
│   │       ├── macro_config.h         既定のマクロ設定
│   │       ├── MacroManager.cpp       マクロ実行エンジン
│   │       └── MacroManager.h         マクロ実行エンジン（ヘッダー）
│   └── src/
│       ├── ConfigStorage.cpp          EEPROM 保存/読込の実装
│       ├── Encoder.cpp                ダイヤル割り込み処理の実装
│       ├── KeyInput.cpp               ボタン検出の実装
│       ├── main.cpp                   メイン処理（setup / loop）
│       ├── SerialConsole.cpp          Serial コマンド処理の実装
│       ├── StatusLed.cpp              LED 表示処理の実装（core1）
│       └── UsbSetup.cpp               HID / MSC / CDC の設定実装
└── PCB(kicad)/
    ├── RP2040_key_board_Gerber.zip    基板製造用ガーバーデータ（Zip）
    ├── RP2040_key_board.kicad_pcb     KiCad 基板レイアウトデータ
    ├── RP2040_key_board.kicad_prl     KiCad プロジェクトローカル設定
    ├── RP2040_key_board.kicad_pro     KiCad プロジェクトファイル
    ├── RP2040_key_board.kicad_sch     KiCad 回路図データ
    ├── RP2040_key_board.pdf           回路図（PDF）
    └── RP2040_key_board.step          基板 3D モデル（STEP）

```

## 内部構成

### コアの役割分担

| コア | 担当 |
|---|---|
| core0(`setup` / `loop`) | USB(`tud_task`)、マクロ実行、Serial コンソール、ボタン・ダイヤル入力 |
| core1(`setup1` / `loop1`) | LED 表示のみ |

### 起動の流れ

1. USB をいったん切断(`detach`)
2. EEPROM から設定を読み込み(無効なら既定値)、`MacroManager` を初期化
3. USB の構成(HID / MSC / CDC)を組み立ててから接続(`attach`)
4. GPIO 設定 → USB の準備完了を待機 → ダイヤルの割り込み登録 → 1 秒待機
5. `isConnected = true`(LED とマクロ入力が通常動作に入る)

### マクロの実行

- 10 個のスイッチはそれぞれ独立したキューを持ち、押した瞬間にステップがキューに積まれて順に実行されます。
- 各スイッチが押しているキーは全スイッチ分を合成して 1 つの HID レポートとして送信されます。
- マクロの終了時に押されたままのキーは自動で解除されます。
- ダイヤルは 4 カウントで 1 クリックとして扱い、1 クリックにつき 1 回マクロを起動します。

### 設定データの形式(364 バイト)

| オフセット | サイズ | 内容 |
|---|---|---|
| 0 〜 3 | 4 B | タイムスタンプ(2026-01-01 00:00:00 UTC からの経過秒、リトルエンディアン) |
| 4 〜 363 | 36 B × 10 | スイッチ 0〜9 のマクロ |

1 スイッチ(36 B)は、4 ステップ分のブロック(9 B)× 4 です。ブロックの先頭 1 B に 4 ステップ分の type(2 bit ずつ)、続く 8 B に各ステップの code(2 B × 4、リトルエンディアン)が入ります。

| type | 意味 | code |
|---|---|---|
| 0 | 押す | HID キーコード |
| 1 | 離す | HID キーコード |
| 2 | 打鍵 | HID キーコード |
| 3 | 制御 | `0` = 全キー解除 / `1〜65535` = ウェイト(ms) |

code の最上位ビット(`0x8000`)が立っている場合はメディアキーで、下位 15 bit が Consumer Usage ID です(種類に関わらず「押して離す」動作)。

EEPROM 上では、この 364 バイトの後ろにマジック `0x4D43`(2 B)と CRC16-CCITT(2 B)が続き、両方が一致したときだけ有効な設定として読み込まれます。

## 注意事項

- `save` はフラッシュを書き換えるため、数十 ms の間キー入力と LED が止まります。書き換え回数には限りがある(目安 約 10 万回)ので、連打は避けてください。
- マクロ実行中の `save` / `erase` は拒否されます。
- フラッシュ全体を消去する `flash_nuke.uf2` を使うと、保存した設定も含め、ファームウェアが完全に消えます。
