/**
 * @file EEPROM.hpp
 * @brief EEPROM Driver - C++17
 * @description 4Kbit (512 Byte) I2C EEPROM Driver
 *              Common device: AT24C04 (4Kbit) or compatible
 *              I2C Address: 0x50 (A2=A1=A0=GND)
 */

#ifndef EEPROM_HPP
#define EEPROM_HPP

#include "main.h"
#include <cstdint>
#include <cstddef>

/*============================================================================
 * 常量定义
 *============================================================================*/

namespace EEPROMConfig {
    // I2C Address (7-bit) - A2,A1,A0 all connected to GND
    constexpr uint8_t I2C_ADDRESS = 0x50;

    // Memory size (4Kbit = 512 Bytes for AT24C04)
    constexpr uint32_t CAPACITY = 512;   // 4Kbit

    // Page size (AT24C04 has 16-byte page)
    constexpr uint16_t PAGE_SIZE = 16;

    // Write cycle time (ms)
    constexpr uint16_t WRITE_CYCLE_MS = 5;

    // Timeout for I2C operations
    constexpr uint32_t TIMEOUT_MS = 1000;
}

/*============================================================================
 * EEPROM 类
 *============================================================================*/

class EEPROM {
public:
    /**
     * @brief 默认构造函数
     */
    EEPROM() = default;

    /**
     * @brief 初始化EEPROM
     * @param hi2c I2C句柄
     * @return true=成功, false=失败
     */
    bool init(I2C_HandleTypeDef* hi2c);

    /**
     * @brief 检查EEPROM是否存在
     * @return true=存在, false=不存在
     */
    bool isConnected();

    /**
     * @brief 读取数据
     * @param addr 起始地址 (0 - CAPACITY-1)
     * @param data 数据缓冲区
     * @param len 读取长度
     * @return true=成功, false=失败
     */
    bool read(uint32_t addr, uint8_t* data, uint32_t len);

    /**
     * @brief 写入数据
     * @param addr 起始地址 (0 - CAPACITY-1)
     * @param data 数据缓冲区
     * @param len 写入长度
     * @return true=成功, false=失败
     */
    bool write(uint32_t addr, const uint8_t* data, uint32_t len);

    /**
     * @brief 读取16位数据 (大端序)
     * @param addr 地址
     * @param value 输出值
     * @return true=成功, false=失败
     */
    bool read16(uint32_t addr, uint16_t& value);

    /**
     * @brief 写入16位数据 (大端序)
     * @param addr 地址
     * @param value 输入值
     * @return true=成功, false=失败
     */
    bool write16(uint32_t addr, uint16_t value);

    /**
     * @brief 读取32位数据 (大端序)
     * @param addr 地址
     * @param value 输出值
     * @return true=成功, false=失败
     */
    bool read32(uint32_t addr, uint32_t& value);

    /**
     * @brief 写入32位数据 (大端序)
     * @param addr 地址
     * @param value 输入值
     * @return true=成功, false=失败
     */
    bool write32(uint32_t addr, uint32_t value);

    /**
     * @brief 读取字符串 (null-terminated)
     * @param addr 起始地址
     * @param str 字符串缓冲区
     * @param maxLen 最大长度
     * @return true=成功, false=失败
     */
    bool readString(uint32_t addr, char* str, uint32_t maxLen);

    /**
     * @brief 写入字符串
     * @param addr 起始地址
     * @param str 字符串 (null-terminated)
     * @return true=成功, false=失败
     */
    bool writeString(uint32_t addr, const char* str);

    /**
     * @brief 格式化EEPROM (写入0xFF)
     * @return true=成功, false=失败
     */
    bool format();

    /**
     * @brief 获取容量
     * @return 容量 (字节)
     */
    constexpr uint32_t capacity() const { return EEPROMConfig::CAPACITY; }

    /**
     * @brief 随机写入单字节 (写入随机值并验证)
     * @param addr 写入地址 (0 - CAPACITY-1)
     * @param value 要写入的值
     * @return true=写入并验证成功, false=失败
     */
    bool writeVerify(uint32_t addr, uint8_t value);

    /**
     * @brief 读取指定地址的单字节
     * @param addr 读取地址
     * @param value 输出值
     * @return true=成功, false=失败
     */
    bool readByte(uint32_t addr, uint8_t& value);

private:
    I2C_HandleTypeDef* m_hi2c = nullptr;

    // 写入单页 (自动处理跨页边界)
    bool writePage(uint16_t pageAddr, const uint8_t* data, uint16_t len);

    // 等待写入完成
    bool waitReady();
};

#endif // EEPROM_HPP