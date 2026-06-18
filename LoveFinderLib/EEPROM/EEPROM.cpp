/**
 * @file EEPROM.cpp
 * @brief EEPROM Driver Implementation - C++17
 */

#include "EEPROM.hpp"
#include "stm32g0xx_hal_i2c.h"
#include <cstring>

/*============================================================================
 * 公共接口实现
 *============================================================================*/

bool EEPROM::init(I2C_HandleTypeDef* hi2c)
{
    m_hi2c = hi2c;
    return isConnected();
}

bool EEPROM::isConnected()
{
    if (m_hi2c == nullptr) {
        return false;
    }

    return HAL_OK == HAL_I2C_IsDeviceReady(
        m_hi2c,
        EEPROMConfig::I2C_ADDRESS << 1,
        3,
        EEPROMConfig::TIMEOUT_MS
    );
}

bool EEPROM::read(uint32_t addr, uint8_t* data, uint32_t len)
{
    if (m_hi2c == nullptr || data == nullptr || len == 0) {
        return false;
    }

    if (addr + len > EEPROMConfig::CAPACITY) {
        return false;
    }

    // 使用HAL_I2C_Mem_Read读取数据
    // I2C_MEMADD_SIZE_8BIT 表示内存地址是8位（对于24C256需要发送两次）
    if (HAL_OK != HAL_I2C_Mem_Read(
        m_hi2c,
        EEPROMConfig::I2C_ADDRESS << 1,
        static_cast<uint16_t>(addr),
        I2C_MEMADD_SIZE_8BIT,
        data,
        len,
        EEPROMConfig::TIMEOUT_MS
    )) {
        return false;
    }

    return true;
}

bool EEPROM::write(uint32_t addr, const uint8_t* data, uint32_t len)
{
    if (m_hi2c == nullptr || data == nullptr || len == 0) {
        return false;
    }

    if (addr + len > EEPROMConfig::CAPACITY) {
        return false;
    }

    uint32_t written = 0;

    while (written < len) {
        // 计算当前页的剩余空间
        uint16_t pageStart = static_cast<uint16_t>(addr + written);
        uint16_t pageOffset = pageStart % EEPROMConfig::PAGE_SIZE;
        uint16_t pageRemaining = EEPROMConfig::PAGE_SIZE - pageOffset;

        // 写入不超过剩余空间或剩余数据量的数据
        uint16_t toWrite = static_cast<uint16_t>(
            (len - written < pageRemaining) ? (len - written) : pageRemaining
        );

        if (!writePage(pageStart, data + written, toWrite)) {
            return false;
        }

        written += toWrite;
    }

    // 等待写入完成
    return waitReady();
}

bool EEPROM::writePage(uint16_t pageAddr, const uint8_t* data, uint16_t len)
{
    if (len > EEPROMConfig::PAGE_SIZE) {
        len = EEPROMConfig::PAGE_SIZE;
    }

    // 构建发送缓冲区: [地址高字节, 地址低字节, 数据...]
    uint8_t buffer[EEPROMConfig::PAGE_SIZE + 2];
    buffer[0] = static_cast<uint8_t>(pageAddr >> 8);
    buffer[1] = static_cast<uint8_t>(pageAddr & 0xFF);
    memcpy(buffer + 2, data, len);

    return HAL_OK == HAL_I2C_Master_Transmit(
        m_hi2c,
        EEPROMConfig::I2C_ADDRESS << 1,
        buffer,
        len + 2,
        EEPROMConfig::TIMEOUT_MS
    );
}

bool EEPROM::waitReady()
{
    // 轮询设备就绪
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < EEPROMConfig::WRITE_CYCLE_MS) {
        if (isConnected()) {
            return true;
        }
    }
    return isConnected();
}

bool EEPROM::read16(uint32_t addr, uint16_t& value)
{
    uint8_t data[2];
    if (read(addr, data, 2)) {
        value = static_cast<uint16_t>(data[0]) << 8 | data[1];
        return true;
    }
    return false;
}

bool EEPROM::write16(uint32_t addr, uint16_t value)
{
    uint8_t data[2];
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value & 0xFF);
    return write(addr, data, 2);
}

bool EEPROM::read32(uint32_t addr, uint32_t& value)
{
    uint8_t data[4];
    if (read(addr, data, 4)) {
        value = static_cast<uint32_t>(data[0]) << 24 |
                static_cast<uint32_t>(data[1]) << 16 |
                static_cast<uint32_t>(data[2]) << 8 |
                data[3];
        return true;
    }
    return false;
}

bool EEPROM::write32(uint32_t addr, uint32_t value)
{
    uint8_t data[4];
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value & 0xFF);
    return write(addr, data, 4);
}

bool EEPROM::readString(uint32_t addr, char* str, uint32_t maxLen)
{
    if (str == nullptr || maxLen == 0) {
        return false;
    }

    uint8_t data;
    uint32_t i = 0;

    while (i < maxLen - 1) {
        if (!read(addr + i, &data, 1)) {
            return false;
        }
        str[i] = static_cast<char>(data);
        if (data == 0) {
            break;
        }
        i++;
    }

    str[i] = 0;
    return true;
}

bool EEPROM::writeString(uint32_t addr, const char* str)
{
    if (str == nullptr) {
        return false;
    }

    uint32_t len = strlen(str) + 1;  // include null terminator
    return write(addr, reinterpret_cast<const uint8_t*>(str), len);
}

bool EEPROM::format()
{
    const uint32_t bufferSize = 16;  // 减小到一页大小，确保写入成功
    uint8_t buffer[bufferSize];
    uint32_t failAddr = 0;

    // 填充0xFF
    memset(buffer, 0xFF, bufferSize);

    // 分页写入
    for (uint32_t addr = 0; addr < EEPROMConfig::CAPACITY; addr += bufferSize) {
        uint32_t len = (addr + bufferSize > EEPROMConfig::CAPACITY) ?
                       (EEPROMConfig::CAPACITY - addr) : bufferSize;

        if (!write(addr, buffer, len)) {
            failAddr = addr;
            (void)failAddr;  // 可用于调试
            return false;
        }
    }

    return true;
}

bool EEPROM::writeVerify(uint32_t addr, uint8_t value)
{
    if (addr >= EEPROMConfig::CAPACITY) {
        return false;
    }

    // 写入单字节
    if (!write(addr, &value, 1)) {
        return false;
    }

    // 读取验证
    uint8_t readValue;
    if (!readByte(addr, readValue)) {
        return false;
    }

    return (readValue == value);
}

bool EEPROM::readByte(uint32_t addr, uint8_t& value)
{
    if (addr >= EEPROMConfig::CAPACITY) {
        return false;
    }

    return read(addr, &value, 1);
}