#ifndef MACRO_MANAGER_H
#define MACRO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <Adafruit_TinyUSB.h>

enum KeyAction : uint8_t {
    ACTION_NONE = 0,
    KEY_DOWN,       // 00: キー押下
    KEY_UP,         // 01: キー解除
    KEY_TAP,        // 10: 通常キーTap
    CMD_DELAY,      // 11: ウェイト (code > 0 の時 ms単位)
    CMD_ALL_RELEASE,// 11: 全キー一括解除 (code == 0 の時)
    MEDIA_TAP       // メディアキーTap
};

struct KeyItem {
    KeyAction action;
    uint16_t code;
};

class MacroManager {
public:
    static constexpr uint8_t SWITCH_COUNT = 10;
    static constexpr uint8_t MACRO_BYTES_PER_SW = 36;
    static constexpr uint8_t MAX_STEPS_PER_SW = 16; // 1スイッチあたり最大16ステップ(4ブロック x 4)
    static constexpr uint16_t TOTAL_CONFIG_SIZE = 364;
    static constexpr uint32_t BASE_TIMESTAMP_2026 = 1767225600U;

    MacroManager();
    
    void init(Adafruit_USBD_HID* hidPtr = nullptr);
    void setHID(Adafruit_USBD_HID* hidPtr) { pHid = hidPtr; }

    bool loadConfig(const uint8_t configData[TOTAL_CONFIG_SIZE]);
    bool triggerMacro(uint8_t switchIndex);
    void processTask();

    uint32_t getRelativeTimestamp() const { return timestamp; }
    void setTimestamp(uint32_t relativeSec) { timestamp = relativeSec; }

    // ---- 設定の取得・編集 (Serialコンソール / EEPROM保存用) ----
    // 現在の設定を364バイトのイメージ(先頭4バイト=タイムスタンプ + 36バイト x 10)で書き出す
    void exportConfig(uint8_t out[TOTAL_CONFIG_SIZE]) const;
    // 1ステップ分を取得/書き換え (sw: 0-9, step: 0-15, type: 0-3, code: 16bit)
    bool getStep(uint8_t sw, uint8_t step, uint8_t &type, uint16_t &code) const;
    bool setStep(uint8_t sw, uint8_t step, uint8_t type, uint16_t code);
    // いずれかのマクロが実行中なら true
    bool isBusy() const;

private:
    // リングバッファは1要素を空きとして使うので、最大ステップ数 + 1 が必要
    // (16だと16ステップ全部を使ったマクロの最後の1ステップが入らなかった)
    static constexpr uint8_t QUEUE_SIZE_PER_SW = MAX_STEPS_PER_SW + 1;

    struct SwitchContext {
        KeyItem queue[QUEUE_SIZE_PER_SW];
        uint8_t head;
        uint8_t tail;
        KeyItem currentItem;
        uint32_t delayEndTime;
        bool delaying; // ディレイ待機中 (delayEndTime == 0 を番兵にしない)
        
        uint8_t localModifiers;
        uint8_t localKeys[6];
        bool isActive;
    };

    SwitchContext swContexts[SWITCH_COUNT];
    Adafruit_USBD_HID* pHid;

    uint8_t globalModifiers;
    uint8_t globalKeys[6];
    bool isReportDirty;

    uint32_t timestamp;
    uint8_t macroStorage[SWITCH_COUNT][MACRO_BYTES_PER_SW];

    bool enqueue(uint8_t swIdx, KeyAction action, uint16_t code);
    bool dequeue(uint8_t swIdx, KeyItem &item);
    bool setMacro36(uint8_t swIdx, const uint8_t macroData[36]);

    void addLocalKey(SwitchContext &ctx, uint8_t code);
    void removeLocalKey(SwitchContext &ctx, uint8_t code);
    void clearLocalKeys(SwitchContext &ctx);
    static bool hasHeldKeys(const SwitchContext &ctx);

    // USBタスクを回しながら待つ (delay()中もHIDの送信完了処理を進めるため)
    void waitMs(uint32_t ms);
    bool waitHidReady(uint32_t timeoutMs);
    bool sendMediaReport(uint16_t usage, uint32_t timeoutMs);

    // 全スイッチの状態を合成してHIDレポートを送る。送れなければ isReportDirty を立てて後で再送
    void updateAndSendGlobalReport(uint32_t readyTimeoutMs = 10);
};

#endif // MACRO_MANAGER_H
