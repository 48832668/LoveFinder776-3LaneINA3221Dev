/**
 * @file AT24C04.cpp
 * @brief AT24C04 EEPROM Driver Implementation - C++17
 */

#include "AT24C04.hpp"

/*============================================================================
 * Private Methods
 *============================================================================*/

/**
 * @brief Get I2C device address based on memory address
 */
uint8_t AT24C04::getDeviceAddr(uint16_t addr)
{
    // Address 0-255: use 0x50, Address 256-511: use 0x51
    return (addr < 256) ? AT24C04Config::I2C_ADDR_PAGE0 : AT24C04Config::I2C_ADDR_PAGE1;
}

/**
 * @brief Wait for write completion (polling)
 */
bool AT24C04::waitReady()
{
    // Poll both devices to ensure write is complete
    for (int i = 0; i < 10; i++) {
        if (m_i2c->isDeviceReady(AT24C04Config::I2C_ADDR_PAGE0)) {
            return true;
        }
        HAL_Delay(1);
    }
    return false;
}

/**
 * @brief Write single page (max 16 bytes)
 */
bool AT24C04::writePage(uint8_t devAddr, uint8_t pageAddr, const uint8_t* data, uint8_t len)
{
    // Limit to 16 bytes per page
    if (len > 16) len = 16;

    // Build buffer: [address, data0, data1, ...]
    uint8_t buf[17];
    buf[0] = pageAddr;
    for (uint8_t i = 0; i < len; i++) {
        buf[i + 1] = data[i];
    }

    // Write via I2C C API
    if (e_I2C_WriteRegs(m_i2c->getHandle(), devAddr, pageAddr, data, len) != e_I2C_Status::OK) {
        return false;
    }

    // Wait for write completion
    HAL_Delay(AT24C04Config::WRITE_CYCLE_MS);
    return waitReady();
}

/*============================================================================
 * Public Methods
 *============================================================================*/

/**
 * @brief Initialize
 */
bool AT24C04::init(I2C_HandleTypeDef* hi2c)
{
    m_i2c = new I2C(hi2c);
    return isConnected();
}

/**
 * @brief Check if connected
 */
bool AT24C04::isConnected()
{
    return m_i2c && (m_i2c->isDeviceReady(AT24C04Config::I2C_ADDR_PAGE0) ||
                     m_i2c->isDeviceReady(AT24C04Config::I2C_ADDR_PAGE1));
}

/**
 * @brief Read data
 */
bool AT24C04::read(uint16_t addr, uint8_t* data, uint16_t len)
{
    if (!m_i2c || addr + len > AT24C04Config::CAPACITY) {
        return false;
    }

    while (len > 0) {
        uint8_t devAddr = getDeviceAddr(addr);
        uint8_t memAddr = static_cast<uint8_t>(addr & 0xFF);

        // Handle cross-page boundary
        uint16_t chunkLen = len;
        if ((addr & 0xFF) + chunkLen > 256) {
            chunkLen = 256 - (addr & 0xFF);
        }

        if (m_i2c->readRegs(devAddr, memAddr, data, chunkLen) != e_I2C_Status::OK) {
            return false;
        }

        addr += chunkLen;
        data += chunkLen;
        len -= chunkLen;
    }

    return true;
}

/**
 * @brief Write data (automatically handles page boundaries)
 */
bool AT24C04::write(uint16_t addr, const uint8_t* data, uint16_t len)
{
    if (!m_i2c || addr + len > AT24C04Config::CAPACITY) {
        return false;
    }

    while (len > 0) {
        uint8_t devAddr = getDeviceAddr(addr);
        uint8_t memAddr = static_cast<uint8_t>(addr & 0xFF);

        // Calculate how many bytes we can write in this page
        // AT24C04 has 16-byte pages, page boundary at 0, 16, 32...
        uint8_t pageOffset = memAddr % AT24C04Config::PAGE_SIZE;
        uint8_t chunkLen = AT24C04Config::PAGE_SIZE - pageOffset;
        if (chunkLen > len) chunkLen = len;
        if (chunkLen > 16) chunkLen = 16;  // Safety limit

        if (!writePage(devAddr, memAddr, data, chunkLen)) {
            return false;
        }

        addr += chunkLen;
        data += chunkLen;
        len -= chunkLen;
    }

    return true;
}

/**
 * @brief Read 8-bit value
 */
bool AT24C04::read8(uint16_t addr, uint8_t& value)
{
    return read(addr, &value, 1);
}

/**
 * @brief Write 8-bit value
 */
bool AT24C04::write8(uint16_t addr, uint8_t value)
{
    return write(addr, &value, 1);
}

/**
 * @brief Read 16-bit value (big-endian)
 */
bool AT24C04::read16(uint16_t addr, uint16_t& value)
{
    uint8_t buf[2];
    if (!read(addr, buf, 2)) return false;
    value = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return true;
}

/**
 * @brief Write 16-bit value (big-endian)
 */
bool AT24C04::write16(uint16_t addr, uint16_t value)
{
    uint8_t buf[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    return write(addr, buf, 2);
}

/**
 * @brief Read 32-bit value (big-endian)
 */
bool AT24C04::read32(uint16_t addr, uint32_t& value)
{
    uint8_t buf[4];
    if (!read(addr, buf, 4)) return false;
    value = (static_cast<uint32_t>(buf[0]) << 24) |
            (static_cast<uint32_t>(buf[1]) << 16) |
            (static_cast<uint32_t>(buf[2]) << 8) |
            buf[3];
    return true;
}

/**
 * @brief Write 32-bit value (big-endian)
 */
bool AT24C04::write32(uint16_t addr, uint32_t value)
{
    uint8_t buf[4] = {
        static_cast<uint8_t>(value >> 24),
        static_cast<uint8_t>(value >> 16),
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value & 0xFF)
    };
    return write(addr, buf, 4);
}

/**
 * @brief Read string
 */
bool AT24C04::readString(uint16_t addr, char* str, uint16_t maxLen)
{
    if (!m_i2c || addr >= AT24C04Config::CAPACITY || maxLen == 0) {
        return false;
    }

    uint8_t devAddr = getDeviceAddr(addr);
    uint8_t memAddr = static_cast<uint8_t>(addr & 0xFF);

    // Read up to maxLen-1 bytes (leave room for null terminator)
    if (m_i2c->readRegs(devAddr, memAddr, reinterpret_cast<uint8_t*>(str), maxLen - 1) != e_I2C_Status::OK) {
        return false;
    }

    str[maxLen - 1] = '\0';
    return true;
}

/**
 * @brief Write string
 */
bool AT24C04::writeString(uint16_t addr, const char* str)
{
    // Calculate string length
    uint16_t len = 0;
    while (str[len] != '\0' && (addr + len) < AT24C04Config::CAPACITY) {
        len++;
    }
    return write(addr, reinterpret_cast<const uint8_t*>(str), len);
}

/*============================================================================
 * C API Implementation
 *============================================================================*/

extern "C" {

bool b_AT24C04_Init(AT24C04* eeprom, I2C_HandleTypeDef* hi2c)
{
    return eeprom->init(hi2c);
}

bool b_AT24C04_IsConnected(AT24C04* eeprom)
{
    return eeprom->isConnected();
}

bool b_AT24C04_Read(AT24C04* eeprom, uint16_t addr, uint8_t* data, uint16_t len)
{
    return eeprom->read(addr, data, len);
}

bool b_AT24C04_Write(AT24C04* eeprom, uint16_t addr, const uint8_t* data, uint16_t len)
{
    return eeprom->write(addr, data, len);
}

bool b_AT24C04_Read8(AT24C04* eeprom, uint16_t addr, uint8_t* value)
{
    return eeprom->read8(addr, *value);
}

bool b_AT24C04_Write8(AT24C04* eeprom, uint16_t addr, uint8_t value)
{
    return eeprom->write8(addr, value);
}

bool b_AT24C04_Read16(AT24C04* eeprom, uint16_t addr, uint16_t* value)
{
    return eeprom->read16(addr, *value);
}

bool b_AT24C04_Write16(AT24C04* eeprom, uint16_t addr, uint16_t value)
{
    return eeprom->write16(addr, value);
}

bool b_AT24C04_ReadString(AT24C04* eeprom, uint16_t addr, char* str, uint16_t maxLen)
{
    return eeprom->readString(addr, str, maxLen);
}

bool b_AT24C04_WriteString(AT24C04* eeprom, uint16_t addr, const char* str)
{
    return eeprom->writeString(addr, str);
}

} // extern "C"