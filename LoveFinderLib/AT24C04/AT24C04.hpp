/**
 * @file AT24C04.hpp
 * @brief AT24C04 EEPROM Driver - C++17
 * @description 4Kbit (512 bytes) I2C EEPROM Driver
 *              Device: AT24C04 (or compatible)
 *              I2C Addresses: 0x50 (page 0), 0x51 (page 1)
 */

#ifndef AT24C04_HPP
#define AT24C04_HPP

#include "main.h"
#include "I2C.hpp"
#include <cstdint>
#include <cstddef>

/*============================================================================
 * Constants
 *============================================================================*/

namespace AT24C04Config {
    // I2C Addresses (7-bit)
    constexpr uint8_t I2C_ADDR_PAGE0 = 0x50;  // 256 bytes (0-255)
    constexpr uint8_t I2C_ADDR_PAGE1 = 0x51;  // 256 bytes (256-511)

    // Total capacity: 512 bytes (4Kbit)
    constexpr uint32_t CAPACITY = 512;

    // Page size: 16 bytes
    constexpr uint16_t PAGE_SIZE = 16;

    // Write cycle time (ms)
    constexpr uint16_t WRITE_CYCLE_MS = 5;
}

/*============================================================================
 * AT24C04 Class
 *============================================================================*/

class AT24C04 {
public:
    /**
     * @brief Default constructor
     */
    AT24C04() = default;

    /**
     * @brief Initialize with I2C handle
     * @param hi2c Pointer to HAL I2C handle (e.g., &hi2c1)
     * @return true = success
     */
    bool init(I2C_HandleTypeDef* hi2c);

    /**
     * @brief Check if EEPROM is connected
     * @return true = connected
     */
    bool isConnected();

    /**
     * @brief Read data from EEPROM
     * @param addr Start address (0 - CAPACITY-1)
     * @param data Data buffer
     * @param len Length to read
     * @return true = success
     */
    bool read(uint16_t addr, uint8_t* data, uint16_t len);

    /**
     * @brief Write data to EEPROM
     * @param addr Start address (0 - CAPACITY-1)
     * @param data Data buffer
     * @param len Length to write
     * @return true = success
     */
    bool write(uint16_t addr, const uint8_t* data, uint16_t len);

    /**
     * @brief Read 8-bit value
     * @param addr Address (0-511)
     * @param value Output value
     * @return true = success
     */
    bool read8(uint16_t addr, uint8_t& value);

    /**
     * @brief Write 8-bit value
     * @param addr Address (0-511)
     * @param value Input value
     * @return true = success
     */
    bool write8(uint16_t addr, uint8_t value);

    /**
     * @brief Read 16-bit value (big-endian)
     * @param addr Address (0-510)
     * @param value Output value
     * @return true = success
     */
    bool read16(uint16_t addr, uint16_t& value);

    /**
     * @brief Write 16-bit value (big-endian)
     * @param addr Address (0-510)
     * @param value Input value
     * @return true = success
     */
    bool write16(uint16_t addr, uint16_t value);

    /**
     * @brief Read 32-bit value (big-endian)
     * @param addr Address (0-508)
     * @param value Output value
     * @return true = success
     */
    bool read32(uint16_t addr, uint32_t& value);

    /**
     * @brief Write 32-bit value (big-endian)
     * @param addr Address (0-508)
     * @param value Input value
     * @return true = success
     */
    bool write32(uint16_t addr, uint32_t value);

    /**
     * @brief Read string (null-terminated)
     * @param addr Start address
     * @param str String buffer
     * @param maxLen Max buffer length
     * @return true = success
     */
    bool readString(uint16_t addr, char* str, uint16_t maxLen);

    /**
     * @brief Write string (null-terminated)
     * @param addr Start address
     * @param str String to write
     * @return true = success
     */
    bool writeString(uint16_t addr, const char* str);

    /**
     * @brief Get capacity
     * @return Capacity in bytes
     */
    constexpr uint32_t capacity() const { return AT24C04Config::CAPACITY; }

private:
    I2C* m_i2c = nullptr;

    // Write single page
    bool writePage(uint8_t devAddr, uint8_t pageAddr, const uint8_t* data, uint8_t len);

    // Wait for write completion
    bool waitReady();

    // Get I2C address based on address
    uint8_t getDeviceAddr(uint16_t addr);
};

/*============================================================================
 * C API Compatibility
 *=======================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

bool b_AT24C04_Init(AT24C04* eeprom, I2C_HandleTypeDef* hi2c);
bool b_AT24C04_IsConnected(AT24C04* eeprom);
bool b_AT24C04_Read(AT24C04* eeprom, uint16_t addr, uint8_t* data, uint16_t len);
bool b_AT24C04_Write(AT24C04* eeprom, uint16_t addr, const uint8_t* data, uint16_t len);
bool b_AT24C04_Read8(AT24C04* eeprom, uint16_t addr, uint8_t* value);
bool b_AT24C04_Write8(AT24C04* eeprom, uint16_t addr, uint8_t value);
bool b_AT24C04_Read16(AT24C04* eeprom, uint16_t addr, uint16_t* value);
bool b_AT24C04_Write16(AT24C04* eeprom, uint16_t addr, uint16_t value);
bool b_AT24C04_ReadString(AT24C04* eeprom, uint16_t addr, char* str, uint16_t maxLen);
bool b_AT24C04_WriteString(AT24C04* eeprom, uint16_t addr, const char* str);

#ifdef __cplusplus
}
#endif

#endif // AT24C04_HPP