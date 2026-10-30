#include "SerialConsole.h"
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "ConfigStorage.h"

namespace
{
    constexpr size_t LINE_MAX_LEN = 1024; // load コマンド(約735文字)が入る大きさ
    constexpr size_t CONFIG_SIZE = MacroManager::TOTAL_CONFIG_SIZE;

    MacroManager *mm = nullptr;
    bool fromEeprom = false; // 起動時にEEPROMから読めたか
    bool dirty = false;      // RAM上の設定がEEPROMと異なる (未保存)
    bool echoOn = true;      // 入力文字をエコーバックする

    char lineBuf[LINE_MAX_LEN + 1];
    size_t lineLen = 0;
    bool lineOverflow = false;

    // マクロ番号の呼び名 (index.html の SW_NAMES と同じ並び。ターミナルで化けないようASCII)
    const char *const SW_NAMES[MacroManager::SWITCH_COUNT] = {
        "top-right key", "bottom-right key", "top-left key", "bottom-left key",
        "upper knob press", "lower knob press",
        "upper knob CW", "upper knob CCW", "lower knob CW", "lower knob CCW"};

    // ------------------------------------------------------------
    // 数値・16進数の解析
    // ------------------------------------------------------------
    bool parseNumber(const char *s, int base, uint32_t &out)
    {
        if (!s || !*s) return false;
        if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
        if (!isalnum((unsigned char)s[0])) return false; // 符号・空白を弾く
        char *end = nullptr;
        unsigned long v = strtoul(s, &end, base);
        if (*end != '\0') return false;
        out = (uint32_t)v;
        return true;
    }

    int hexVal(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // ------------------------------------------------------------
    // 表示
    // ------------------------------------------------------------
    void printTimestampLine()
    {
        time_t t = (time_t)mm->getRelativeTimestamp() + (time_t)MacroManager::BASE_TIMESTAMP_2026;
        struct tm *tmInfo = gmtime(&t);
        char buf[26];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tmInfo);
        // 2026年1月1日からの経過秒数に基準を足した UNIX 時刻と、UTC日時を表示
        Serial.printf("Current timestamp: %lu (%s UTC)\n", (unsigned long)t, buf);
    }

    void printStepLine(uint8_t sw, uint8_t step)
    {
        uint8_t type;
        uint16_t code;
        mm->getStep(sw, step, type, code);

        if (type == 3)
        {
            if (code == 0) Serial.printf("  step %2u: rel                  (all release)\n", step + 1);
            else Serial.printf("  step %2u: wait %-5u ms\n", step + 1, code);
            return;
        }
        if ((code & 0x7FFF) == 0)
        {
            return; // 未設定は表示しない
        }
        static const char *const KIND[] = {"down", "up", "tap"};
        Serial.printf("  step %2u: %-4s 0x%04X%s\n", step + 1, KIND[type], code,
                      (code & 0x8000) ? "  (media key: always tapped)" : "");
    }

    void printSwitch(uint8_t sw)
    {
        Serial.printf("SW%u [%s]\n", sw, SW_NAMES[sw]);
        bool any = false;
        for (uint8_t st = 0; st < MacroManager::MAX_STEPS_PER_SW; st++)
        {
            uint8_t type;
            uint16_t code;
            mm->getStep(sw, st, type, code);
            if (type == 3 || (code & 0x7FFF) != 0)
            {
                printStepLine(sw, st);
                any = true;
            }
        }
        if (!any) Serial.println("  (empty)");
    }

    void printHelp()
    {
        Serial.println(
            "Commands (hex = 0x prefix optional, sw = 0-9, step = 1-16):\n"
            "  help                             this list\n"
            "  status                           config source / unsaved changes / timestamp\n"
            "  show [sw]                        decode macros (all or one switch)\n"
            "  dump                             364 bytes as 728 hex chars (re-loadable)\n"
            "  set <sw> <step> down <hex>       key press        (e.g. set 0 1 down E0)\n"
            "  set <sw> <step> up   <hex>       key release\n"
            "  set <sw> <step> tap  <hex>       press + release  (media: 0x80xx)\n"
            "  set <sw> <step> rel              release all keys\n"
            "  set <sw> <step> wait <ms>        wait, DECIMAL ms 1-65535\n"
            "  set <sw> <step> clear            empty the step\n"
            "  load <728 hex chars>             replace whole config (RAM only)\n"
            "  time <unix seconds>              set config timestamp (RAM only)\n"
            "  save                             write RAM config to EEPROM\n"
            "  reload                           discard edits, re-read EEPROM\n"
            "  erase yes                        wipe EEPROM, go back to built-in defaults\n"
            "  echo on|off                      echo typed characters\n"
            "Edits apply to the next key press immediately; 'save' makes them permanent.\n"
            "Lines starting with '[' are firmware logs. Replies start with OK or ERR.");
    }

    void printStatus()
    {
        Serial.printf("Config source: %s\n", fromEeprom ? "EEPROM" : "built-in default");
        Serial.printf("Unsaved changes: %s\n", dirty ? "yes (run 'save')" : "no");
        printTimestampLine();
    }

    // ------------------------------------------------------------
    // コマンド実装
    // ------------------------------------------------------------
    bool parseSw(const char *s, uint8_t &sw)
    {
        uint32_t v;
        if (!parseNumber(s, 10, v) || v >= MacroManager::SWITCH_COUNT) return false;
        sw = (uint8_t)v;
        return true;
    }

    void cmdShow(int argc, char **argv)
    {
        if (argc >= 2)
        {
            uint8_t sw;
            if (!parseSw(argv[1], sw)) { Serial.println("ERR: sw must be 0-9"); return; }
            printSwitch(sw);
            return;
        }
        for (uint8_t s = 0; s < MacroManager::SWITCH_COUNT; s++) printSwitch(s);
    }

    void cmdDump()
    {
        uint8_t buf[CONFIG_SIZE];
        mm->exportConfig(buf);
        static char hex[CONFIG_SIZE * 2 + 1];
        static const char *const H = "0123456789ABCDEF";
        for (size_t i = 0; i < CONFIG_SIZE; i++)
        {
            hex[i * 2] = H[buf[i] >> 4];
            hex[i * 2 + 1] = H[buf[i] & 0x0F];
        }
        hex[CONFIG_SIZE * 2] = '\0';
        Serial.println(hex);
    }

    void cmdSet(int argc, char **argv)
    {
        if (argc < 4)
        {
            Serial.println("ERR: usage: set <sw> <step> <down|up|tap|rel|wait|clear> [value]");
            return;
        }
        uint8_t sw;
        uint32_t step;
        if (!parseSw(argv[1], sw)) { Serial.println("ERR: sw must be 0-9"); return; }
        if (!parseNumber(argv[2], 10, step) || step < 1 || step > MacroManager::MAX_STEPS_PER_SW)
        {
            Serial.println("ERR: step must be 1-16");
            return;
        }

        const char *kind = argv[3];
        uint8_t type;
        uint32_t code = 0;

        if (!strcasecmp(kind, "down") || !strcasecmp(kind, "up") || !strcasecmp(kind, "tap"))
        {
            type = !strcasecmp(kind, "down") ? 0 : (!strcasecmp(kind, "up") ? 1 : 2);
            if (argc < 5 || !parseNumber(argv[4], 16, code) || code > 0xFFFF)
            {
                Serial.println("ERR: <hex> key code required (0001-FFFF)");
                return;
            }
            if ((code & 0x7FFF) == 0)
            {
                Serial.println("ERR: code is empty; use 'clear'");
                return;
            }
            if (!(code & 0x8000) && code > 0xFF)
            {
                Serial.println("ERR: keyboard codes are 1-byte (00-FF); media keys use 0x8000 | usage");
                return;
            }
        }
        else if (!strcasecmp(kind, "rel"))
        {
            type = 3;
            code = 0;
        }
        else if (!strcasecmp(kind, "wait"))
        {
            type = 3;
            if (argc < 5 || !parseNumber(argv[4], 10, code) || code < 1 || code > 65535)
            {
                Serial.println("ERR: wait needs decimal ms 1-65535");
                return;
            }
        }
        else if (!strcasecmp(kind, "clear"))
        {
            type = 0;
            code = 0;
        }
        else
        {
            Serial.println("ERR: kind must be down|up|tap|rel|wait|clear");
            return;
        }

        mm->setStep(sw, (uint8_t)(step - 1), type, (uint16_t)code);
        dirty = true;
        Serial.println("OK (not saved yet: run 'save')");
        printStepLine(sw, (uint8_t)(step - 1));
    }

    // "load " 以降の文字列から 16進数を読む (空白は無視)
    void cmdLoad(const char *rest)
    {
        uint8_t buf[CONFIG_SIZE];
        size_t nibbles = 0;
        for (const char *p = rest; p && *p; p++)
        {
            if (isspace((unsigned char)*p)) continue;
            int v = hexVal(*p);
            if (v < 0)
            {
                Serial.println("ERR: non-hex character in data");
                return;
            }
            if (nibbles >= CONFIG_SIZE * 2)
            {
                Serial.println("ERR: too long (need exactly 728 hex chars)");
                return;
            }
            if (nibbles % 2 == 0) buf[nibbles / 2] = (uint8_t)(v << 4);
            else buf[nibbles / 2] |= (uint8_t)v;
            nibbles++;
        }
        if (nibbles != CONFIG_SIZE * 2)
        {
            Serial.printf("ERR: need exactly 728 hex chars, got %u\n", (unsigned)nibbles);
            return;
        }
        if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF && buf[3] == 0xFF)
        {
            Serial.println("ERR: timestamp 0xFFFFFFFF means 'blank'; use a real timestamp");
            return;
        }
        mm->loadConfig(buf);
        dirty = true;
        Serial.println("OK (not saved yet: run 'save')");
        printTimestampLine();
    }

    void cmdTime(int argc, char **argv)
    {
        if (argc < 2)
        {
            printTimestampLine();
            return;
        }
        uint32_t unixSec;
        if (!parseNumber(argv[1], 10, unixSec) || unixSec < MacroManager::BASE_TIMESTAMP_2026 ||
            unixSec - MacroManager::BASE_TIMESTAMP_2026 == 0xFFFFFFFFu)
        {
            Serial.println("ERR: need decimal UNIX seconds >= 1767225600 (2026-01-01 UTC)");
            return;
        }
        mm->setTimestamp(unixSec - MacroManager::BASE_TIMESTAMP_2026);
        dirty = true;
        Serial.println("OK (not saved yet: run 'save')");
        printTimestampLine();
    }

    void cmdSave()
    {
        if (mm->isBusy())
        {
            Serial.println("ERR: a macro is running; try again");
            return;
        }
        uint8_t buf[CONFIG_SIZE];
        mm->exportConfig(buf);
        if (ConfigStorage::save(buf))
        {
            dirty = false;
            fromEeprom = true;
            Serial.println("OK saved to EEPROM");
        }
        else
        {
            Serial.println("ERR: EEPROM commit failed");
        }
    }

    void cmdReload()
    {
        uint8_t buf[CONFIG_SIZE];
        bool valid = ConfigStorage::load(buf);
        mm->loadConfig(buf); // 無効なら内蔵の既定値になる
        fromEeprom = valid;
        dirty = false;
        Serial.printf("OK reloaded (%s)\n", valid ? "EEPROM" : "built-in default");
    }

    void cmdErase(int argc, char **argv)
    {
        if (argc < 2 || strcasecmp(argv[1], "yes") != 0)
        {
            Serial.println("ERR: this wipes the saved config. Run 'erase yes' to confirm");
            return;
        }
        if (mm->isBusy())
        {
            Serial.println("ERR: a macro is running; try again");
            return;
        }
        if (!ConfigStorage::erase())
        {
            Serial.println("ERR: EEPROM commit failed");
            return;
        }
        uint8_t blank[CONFIG_SIZE];
        memset(blank, 0xFF, sizeof(blank));
        mm->loadConfig(blank);
        fromEeprom = false;
        dirty = false;
        Serial.println("OK erased; using built-in default");
    }

    // 1行を解釈して実行
    void execute(char *line)
    {
        // 先頭の空白を飛ばす
        while (*line && isspace((unsigned char)*line)) line++;
        if (!*line) return;

        // load はコマンド名より後ろ全部を1つのデータとして扱うので、分割前の行から位置を求める
        bool isLoad = (strncasecmp(line, "load", 4) == 0) && (line[4] == '\0' || isspace((unsigned char)line[4]));
        const char *loadData = isLoad ? line + 4 : nullptr;

        // トークン分割は作業用コピーに対して行う
        char *argv[8];
        int argc = 0;
        char work[LINE_MAX_LEN + 1];
        strncpy(work, line, LINE_MAX_LEN);
        work[LINE_MAX_LEN] = '\0';
        char *save = nullptr;
        for (char *tok = strtok_r(work, " \t", &save); tok && argc < 8; tok = strtok_r(nullptr, " \t", &save))
        {
            argv[argc++] = tok;
        }
        if (argc == 0) return;

        const char *cmd = argv[0];
        if (!strcasecmp(cmd, "help") || !strcmp(cmd, "?")) printHelp();
        else if (!strcasecmp(cmd, "status")) printStatus();
        else if (!strcasecmp(cmd, "show")) cmdShow(argc, argv);
        else if (!strcasecmp(cmd, "dump")) cmdDump();
        else if (!strcasecmp(cmd, "set")) cmdSet(argc, argv);
        else if (isLoad) cmdLoad(loadData);
        else if (!strcasecmp(cmd, "time")) cmdTime(argc, argv);
        else if (!strcasecmp(cmd, "save")) cmdSave();
        else if (!strcasecmp(cmd, "reload")) cmdReload();
        else if (!strcasecmp(cmd, "erase")) cmdErase(argc, argv);
        else if (!strcasecmp(cmd, "echo"))
        {
            if (argc >= 2 && !strcasecmp(argv[1], "on")) echoOn = true;
            else if (argc >= 2 && !strcasecmp(argv[1], "off")) echoOn = false;
            else { Serial.println("ERR: echo on|off"); return; }
            Serial.println("OK");
        }
        else Serial.printf("ERR: unknown command '%s' (type 'help')\n", cmd);
    }

    // 1文字受信
    void feed(char c)
    {
        if (c == '\r' || c == '\n')
        {
            if (echoOn) Serial.print("\r\n");
            if (lineOverflow)
            {
                Serial.printf("ERR: line too long (max %u chars)\n", (unsigned)LINE_MAX_LEN);
            }
            else if (lineLen > 0)
            {
                lineBuf[lineLen] = '\0';
                execute(lineBuf);
            }
            lineLen = 0;
            lineOverflow = false;
            return;
        }
        if (c == 0x08 || c == 0x7F) // Backspace / Delete
        {
            if (lineLen > 0 && !lineOverflow)
            {
                lineLen--;
                if (echoOn) Serial.print("\b \b");
            }
            return;
        }
        if (!isprint((unsigned char)c)) return;

        if (lineLen >= LINE_MAX_LEN)
        {
            lineOverflow = true;
            return;
        }
        lineBuf[lineLen++] = c;
        if (echoOn) Serial.write(c);
    }

    // Serial接続時の案内
    void announceIfConnected()
    {
        static bool connected = false;
        if (connected != tud_cdc_connected())
        {
            connected = tud_cdc_connected();
            if (connected)
            {
                delay(50);
                Serial.println("Serial connected");
                Serial.println("RP2040 keyboard Composite Device");
                printTimestampLine();
                Serial.printf("Config source: %s\n", fromEeprom ? "EEPROM" : "built-in default");
                Serial.println("Type 'help' for config commands.");
                lineLen = 0;
                lineOverflow = false;
            }
        }
    }
}

void consoleBegin(MacroManager &macro, bool configFromEeprom)
{
    mm = &macro;
    fromEeprom = configFromEeprom;
    dirty = false;
}

void consolePoll()
{
    announceIfConnected();
    while (Serial.available() > 0)
    {
        int c = Serial.read();
        if (c < 0) break;
        feed((char)c);
    }
}
