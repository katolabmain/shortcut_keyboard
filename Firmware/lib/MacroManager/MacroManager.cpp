#include "MacroManager.h"
#include "pico/stdlib.h"
#include <Arduino.h>
#include <string.h>
#include "macro_config.h"
MacroManager::MacroManager() : pHid(nullptr), globalModifiers(0), isReportDirty(false), timestamp(0) {
    memset(globalKeys, 0, sizeof(globalKeys));
    memset(macroStorage, 0, sizeof(macroStorage));
    init();
}

void MacroManager::init(Adafruit_USBD_HID* hidPtr) {
    pHid = hidPtr;
    globalModifiers = 0;
    isReportDirty = false;
    memset(globalKeys, 0, sizeof(globalKeys));

    for (int i = 0; i < SWITCH_COUNT; i++) {
        swContexts[i].head = 0;
        swContexts[i].tail = 0;
        swContexts[i].currentItem = { ACTION_NONE, 0 };
        swContexts[i].delayEndTime = 0;
        swContexts[i].delaying = false;
        swContexts[i].localModifiers = 0;
        swContexts[i].isActive = false;
        memset(swContexts[i].localKeys, 0, sizeof(swContexts[i].localKeys));
    }
}

bool MacroManager::loadConfig(const uint8_t configData[TOTAL_CONFIG_SIZE]) {
    uint32_t rawTimestamp = (uint32_t)configData[0] |
                            ((uint32_t)configData[1] << 8) |
                            ((uint32_t)configData[2] << 16) |
                            ((uint32_t)configData[3] << 24);

    if (rawTimestamp == 0xFFFFFFFF) {
        // backup dataをセット
        memcpy(macroStorage, &MACRO_CONFIG_DATA[4], SWITCH_COUNT * MACRO_BYTES_PER_SW);
        timestamp = 0;
        return false;
    }

    timestamp = rawTimestamp;
    memcpy(macroStorage, &configData[4], SWITCH_COUNT * MACRO_BYTES_PER_SW);
    return true;
}

bool MacroManager::triggerMacro(uint8_t switchIndex) {
    if (switchIndex >= SWITCH_COUNT) return false;
    
    // 既に該当マクロが実行中の場合は二重起動（連打割り込み）を無視して安全に保護
    if (swContexts[switchIndex].isActive) {
        return false;
    }

    Serial.printf("[Macro] Trigger SW %d\n", switchIndex);

    SwitchContext &ctx = swContexts[switchIndex];
    clearLocalKeys(ctx);
    ctx.head = 0;
    ctx.tail = 0;
    ctx.currentItem = { ACTION_NONE, 0 };
    ctx.delaying = false;

    if (!setMacro36(switchIndex, macroStorage[switchIndex])) {
        // キューに積めなかった場合は中途半端に実行せず破棄する
        ctx.head = 0;
        ctx.tail = 0;
        return false;
    }
    ctx.isActive = true;
    return true;
}

bool MacroManager::enqueue(uint8_t swIdx, KeyAction action, uint16_t code) {
    SwitchContext &ctx = swContexts[swIdx];
    uint8_t nextHead = (ctx.head + 1) % QUEUE_SIZE_PER_SW;
    if (nextHead == ctx.tail) return false;

    ctx.queue[ctx.head].action = action;
    ctx.queue[ctx.head].code = code;
    ctx.head = nextHead;
    return true;
}

bool MacroManager::dequeue(uint8_t swIdx, KeyItem &item) {
    SwitchContext &ctx = swContexts[swIdx];
    if (ctx.head == ctx.tail) return false;
    item = ctx.queue[ctx.tail];
    ctx.tail = (ctx.tail + 1) % QUEUE_SIZE_PER_SW;
    return true;
}

bool MacroManager::setMacro36(uint8_t swIdx, const uint8_t macroData[36]) {
    for (int block = 0; block < 4; block++) {
        const uint8_t* b = &macroData[block * 9];
        uint8_t types = b[0];

        uint16_t codes[4] = {
            (uint16_t)(b[1] | (b[2] << 8)),
            (uint16_t)(b[3] | (b[4] << 8)),
            (uint16_t)(b[5] | (b[6] << 8)),
            (uint16_t)(b[7] | (b[8] << 8))
        };

        for (int i = 0; i < 4; i++) {
            uint8_t type = (types >> (i * 2)) & 0x03;
            uint16_t code = codes[i];
            KeyAction act = ACTION_NONE;

            bool isMedia = (code & 0x8000) != 0;

            // 未設定ステップ(キー/メディア系でコードが空)はスキップ
            if (type != 3 && (code & 0x7FFF) == 0) continue;

            switch (type) {
                case 0:
                    act = isMedia ? MEDIA_TAP : KEY_DOWN;
                    break;
                case 1: 
                    act = isMedia ? MEDIA_TAP : KEY_UP; 
                    break;
                case 2: 
                    act = isMedia ? MEDIA_TAP : KEY_TAP; 
                    break;
                case 3: 
                    act = (code == 0) ? CMD_ALL_RELEASE : CMD_DELAY; 
                    break;
            }

            if (!enqueue(swIdx, act, code)) return false;
        }
    }
    return true;
}

void MacroManager::addLocalKey(SwitchContext &ctx, uint8_t code) {
    if (code >= 0xE0 && code <= 0xE7) {
        ctx.localModifiers |= (1 << (code - 0xE0));
        return;
    }
    // 先に重複チェック (途中に空きがあっても二重登録しない)
    for (int i = 0; i < 6; i++) {
        if (ctx.localKeys[i] == code) return;
    }
    for (int i = 0; i < 6; i++) {
        if (ctx.localKeys[i] == 0) {
            ctx.localKeys[i] = code;
            return;
        }
    }
}

void MacroManager::removeLocalKey(SwitchContext &ctx, uint8_t code) {
    if (code >= 0xE0 && code <= 0xE7) {
        ctx.localModifiers &= ~(1 << (code - 0xE0));
        return;
    }
    for (int i = 0; i < 6; i++) {
        if (ctx.localKeys[i] == code) {
            ctx.localKeys[i] = 0;
        }
    }
}

// 修飾キーまたは通常キーが1つでも押されたままか
// (localKeys[0] だけを見ると、先頭が空いて2番目以降が残った場合に押しっぱなしになる)
bool MacroManager::hasHeldKeys(const SwitchContext &ctx) {
    if (ctx.localModifiers != 0) return true;
    for (int i = 0; i < 6; i++) {
        if (ctx.localKeys[i] != 0) return true;
    }
    return false;
}

// delay() の代わり: 待ち時間中もUSBタスクを回し、直前のレポート送信完了を進める
void MacroManager::waitMs(uint32_t ms) {
    uint32_t start = millis();
    do {
        tud_task();
    } while (millis() - start < ms);
}

// HIDエンドポイントが空くまで待つ (timeoutMs 以内に空かなければ false)
bool MacroManager::waitHidReady(uint32_t timeoutMs) {
    if (!pHid) return false;
    uint32_t start = millis();
    while (!pHid->ready()) {
        tud_task();
        if (millis() - start >= timeoutMs) return false;
    }
    return true;
}

bool MacroManager::sendMediaReport(uint16_t usage, uint32_t timeoutMs) {
    if (!waitHidReady(timeoutMs)) return false;
    return pHid->sendReport16(2, usage);
}

void MacroManager::clearLocalKeys(SwitchContext &ctx) {
    ctx.localModifiers = 0;
    memset(ctx.localKeys, 0, sizeof(ctx.localKeys));
}

void MacroManager::updateAndSendGlobalReport(uint32_t readyTimeoutMs) {
    if (!pHid) return;

    uint8_t mergedModifiers = 0;
    uint8_t mergedKeys[6] = {0};
    uint8_t keyIdx = 0;

    for (int s = 0; s < SWITCH_COUNT; s++) {
        mergedModifiers |= swContexts[s].localModifiers;

        for (int k = 0; k < 6; k++) {
            uint8_t key = swContexts[s].localKeys[k];
            if (key != 0) {
                bool exists = false;
                for (int m = 0; m < keyIdx; m++) {
                    if (mergedKeys[m] == key) { exists = true; break; }
                }
                if (!exists && keyIdx < 6) {
                    mergedKeys[keyIdx++] = key;
                }
            }
        }
    }

    globalModifiers = mergedModifiers;
    memcpy(globalKeys, mergedKeys, 6);

    // 送信できなかった場合は状態を捨てずに dirty を立て、processTask() の末尾で再送する
    // (以前は黙って return しており、キーの押下/解除が失われて押しっぱなしになり得た)
    if (!waitHidReady(readyTimeoutMs) ||
        !pHid->keyboardReport(1, globalModifiers, globalKeys)) {
        isReportDirty = true;
        return;
    }
    isReportDirty = false;

    Serial.printf("[HID Send] Mod: 0x%02X | Keys: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                  globalModifiers, globalKeys[0], globalKeys[1], globalKeys[2],
                  globalKeys[3], globalKeys[4], globalKeys[5]);
}

void MacroManager::processTask() {
    uint32_t nowMs = to_ms_since_boot(get_absolute_time());

    for (int s = 0; s < SWITCH_COUNT; s++) {
        SwitchContext &ctx = swContexts[s];

        // ディレイ待機中 (符号付き差分で比較するので millis のオーバーフローを跨いでも正しい)
        if (ctx.delaying) {
            if ((int32_t)(nowMs - ctx.delayEndTime) < 0) {
                continue;
            }
            ctx.delaying = false;
            ctx.currentItem.action = ACTION_NONE;
        }

        if (ctx.currentItem.action == ACTION_NONE) {
            if (!dequeue(s, ctx.currentItem)) {
                if (ctx.isActive) {
                    if (hasHeldKeys(ctx)) {
                        clearLocalKeys(ctx);
                        updateAndSendGlobalReport();
                        Serial.printf("[Macro] SW %d finished. Auto released keys.\n", s);
                    }
                    ctx.isActive = false;
                }
                continue;
            }
        }

        switch (ctx.currentItem.action) {
            case KEY_DOWN:
                addLocalKey(ctx, (uint8_t)ctx.currentItem.code);
                updateAndSendGlobalReport(); // 即座にHIDレポート送信
                waitMs(12); // OSが押下を認識するための最小ミリ秒ウェイト
                ctx.currentItem.action = ACTION_NONE;
                break;

            case KEY_UP:
                removeLocalKey(ctx, (uint8_t)ctx.currentItem.code);
                updateAndSendGlobalReport(); // 即座にHIDレポート送信
                waitMs(12);
                ctx.currentItem.action = ACTION_NONE;
                break;

            case KEY_TAP:
                addLocalKey(ctx, (uint8_t)ctx.currentItem.code);
                updateAndSendGlobalReport();
                waitMs(20);
                removeLocalKey(ctx, (uint8_t)ctx.currentItem.code);
                updateAndSendGlobalReport();
                waitMs(12);
                ctx.currentItem.action = ACTION_NONE;
                break;

            case MEDIA_TAP: {
                uint16_t usageCode = ctx.currentItem.code & 0x7FFF;
                // 押下が送れなければ ACTION を残して次回再試行
                if (sendMediaReport(usageCode, 10)) {
                    Serial.printf("[Media Send] Code: 0x%04X\n", usageCode);
                    waitMs(35);
                    // 解除は必ず送る (失敗するとメディアキーが押しっぱなしになる)
                    sendMediaReport(0, 100);
                    Serial.println("[Media Send] Released (0x0000)");
                    waitMs(15);
                    ctx.currentItem.action = ACTION_NONE;
                }
                break;
            }

            case CMD_ALL_RELEASE:
                clearLocalKeys(ctx);
                updateAndSendGlobalReport();
                waitMs(12);
                ctx.currentItem.action = ACTION_NONE;
                break;

            case CMD_DELAY:
                Serial.printf("[Delay] SW %d waiting %d ms\n", s, ctx.currentItem.code);
                ctx.delayEndTime = nowMs + ctx.currentItem.code;
                ctx.delaying = true;
                break;

            default:
                ctx.currentItem.action = ACTION_NONE;
                break;
        }
    }

    // 送れなかったレポートの再送 (待たずに1回だけ試す)
    if (isReportDirty) {
        updateAndSendGlobalReport(0);
    }
}

// ---------------------------------------------------------------
// 設定の取得・編集
// ---------------------------------------------------------------
void MacroManager::exportConfig(uint8_t out[TOTAL_CONFIG_SIZE]) const {
    out[0] = (uint8_t)(timestamp & 0xFF);
    out[1] = (uint8_t)((timestamp >> 8) & 0xFF);
    out[2] = (uint8_t)((timestamp >> 16) & 0xFF);
    out[3] = (uint8_t)((timestamp >> 24) & 0xFF);
    memcpy(&out[4], macroStorage, SWITCH_COUNT * MACRO_BYTES_PER_SW);
}

bool MacroManager::getStep(uint8_t sw, uint8_t step, uint8_t &type, uint16_t &code) const {
    if (sw >= SWITCH_COUNT || step >= MAX_STEPS_PER_SW) return false;
    const uint8_t* b = &macroStorage[sw][(step / 4) * 9];
    uint8_t slot = step % 4;
    type = (b[0] >> (slot * 2)) & 0x03;
    code = (uint16_t)(b[1 + slot * 2] | (b[2 + slot * 2] << 8));
    return true;
}

bool MacroManager::setStep(uint8_t sw, uint8_t step, uint8_t type, uint16_t code) {
    if (sw >= SWITCH_COUNT || step >= MAX_STEPS_PER_SW || type > 3) return false;
    uint8_t* b = &macroStorage[sw][(step / 4) * 9];
    uint8_t slot = step % 4;
    b[0] = (uint8_t)((b[0] & ~(0x03 << (slot * 2))) | (type << (slot * 2)));
    b[1 + slot * 2] = (uint8_t)(code & 0xFF);
    b[2 + slot * 2] = (uint8_t)(code >> 8);
    return true;
}

bool MacroManager::isBusy() const {
    for (int s = 0; s < SWITCH_COUNT; s++) {
        if (swContexts[s].isActive) return true;
    }
    return false;
}
