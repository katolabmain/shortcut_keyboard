#include "ConfigStorage.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include "MacroManager.h"

namespace
{
    constexpr size_t EEPROM_SIZE = 512; // EEPROM.begin() に渡す確保サイズ (最大4096)
    constexpr size_t OFFSET_MAGIC = ConfigStorage::CONFIG_SIZE;
    constexpr size_t OFFSET_CRC = OFFSET_MAGIC + 2;
    constexpr size_t USED_SIZE = OFFSET_CRC + 2;
    constexpr uint16_t MAGIC = 0x4D43;

    static_assert(ConfigStorage::CONFIG_SIZE == MacroManager::TOTAL_CONFIG_SIZE,
                  "MacroManager の設定サイズと一致させること");
    static_assert(USED_SIZE <= EEPROM_SIZE, "EEPROM_SIZE が足りない");

    // CRC16-CCITT (poly 0x1021, init 0xFFFF)
    uint16_t crc16(const uint8_t *data, size_t len)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++)
        {
            crc ^= (uint16_t)data[i] << 8;
            for (int b = 0; b < 8; b++)
            {
                crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
            }
        }
        return crc;
    }

    void writeU16(size_t addr, uint16_t v)
    {
        EEPROM.write(addr, v & 0xFF);
        EEPROM.write(addr + 1, v >> 8);
    }

    uint16_t readU16(size_t addr)
    {
        return (uint16_t)(EEPROM.read(addr) | (EEPROM.read(addr + 1) << 8));
    }
}

namespace ConfigStorage
{
    void begin()
    {
        EEPROM.begin(EEPROM_SIZE);
    }

    bool load(uint8_t out[CONFIG_SIZE])
    {
        for (size_t i = 0; i < CONFIG_SIZE; i++)
        {
            out[i] = EEPROM.read(i);
        }

        bool valid = (readU16(OFFSET_MAGIC) == MAGIC) &&
                     (readU16(OFFSET_CRC) == crc16(out, CONFIG_SIZE));
        if (!valid)
        {
            memset(out, 0xFF, CONFIG_SIZE);
        }
        return valid;
    }

    bool save(const uint8_t in[CONFIG_SIZE])
    {
        for (size_t i = 0; i < CONFIG_SIZE; i++)
        {
            EEPROM.write(i, in[i]); // 値が変わらない場合は内部でスキップされる
        }
        writeU16(OFFSET_MAGIC, MAGIC);
        writeU16(OFFSET_CRC, crc16(in, CONFIG_SIZE));
        return EEPROM.commit(); // ここで実際にフラッシュへ書く (数十ms他処理が止まる)
    }

    bool erase()
    {
        for (size_t i = 0; i < USED_SIZE; i++)
        {
            EEPROM.write(i, 0xFF);
        }
        return EEPROM.commit();
    }
}
