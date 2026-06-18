/**
 * @file I2C.cpp
 * @brief Generic I2C Driver Implementation - C++17
 */

#include "I2C.hpp"

/*============================================================================
 * C++ Class Implementation
 *============================================================================*/

/**
 * @brief Write single byte to register
 */
e_I2C_Status I2C::writeReg(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    uint8_t buf[2] = {regAddr, data};
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, buf, 2, TIMEOUT_MS) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Write word (16-bit) to registers (MSB first)
 */
e_I2C_Status I2C::writeReg16(uint8_t devAddr, uint8_t regAddr, uint16_t data)
{
    uint8_t buf[3] = {regAddr, static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data & 0xFF)};
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, buf, 3, TIMEOUT_MS) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Write multiple bytes
 */
e_I2C_Status I2C::writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* data, size_t len)
{
    // Allocate buffer: [regAddr, data0, data1, ...]
    uint8_t* buf = new uint8_t[len + 1];
    buf[0] = regAddr;
    for (size_t i = 0; i < len; i++) {
        buf[i + 1] = data[i];
    }

    e_I2C_Status status = e_I2C_Status::ERROR;
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, buf, len + 1, TIMEOUT_MS) == HAL_OK) {
        status = e_I2C_Status::OK;
    }

    delete[] buf;
    return status;
}

/**
 * @brief Write directly to device (no register)
 */
e_I2C_Status I2C::write(uint8_t devAddr, const uint8_t* data, size_t len)
{
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, const_cast<uint8_t*>(data), len, TIMEOUT_MS) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Read single byte from register
 */
e_I2C_Status I2C::readReg(uint8_t devAddr, uint8_t regAddr, uint8_t* data)
{
    // Write register address, then read
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, &regAddr, 1, TIMEOUT_MS) != HAL_OK) {
        return e_I2C_Status::ERROR;
    }

    if (HAL_I2C_Master_Receive(m_hi2c, devAddr << 1, data, 1, TIMEOUT_MS) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Read word (16-bit) from registers (MSB first)
 */
e_I2C_Status I2C::readReg16(uint8_t devAddr, uint8_t regAddr, uint16_t* data)
{
    uint8_t buf[2];

    // Write register address
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, &regAddr, 1, TIMEOUT_MS) != HAL_OK) {
        return e_I2C_Status::ERROR;
    }

    // Read 2 bytes
    if (HAL_I2C_Master_Receive(m_hi2c, devAddr << 1, buf, 2, TIMEOUT_MS) == HAL_OK) {
        *data = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Read multiple bytes
 */
e_I2C_Status I2C::readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* data, size_t len)
{
    // Write register address
    if (HAL_I2C_Master_Transmit(m_hi2c, devAddr << 1, &regAddr, 1, TIMEOUT_MS) != HAL_OK) {
        return e_I2C_Status::ERROR;
    }

    // Read len bytes
    if (HAL_I2C_Master_Receive(m_hi2c, devAddr << 1, data, len, TIMEOUT_MS) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Read directly from device (no register)
 */
e_I2C_Status I2C::read(uint8_t devAddr, uint8_t* data, size_t len)
{
    if (HAL_I2C_Master_Receive(m_hi2c, devAddr << 1, data, len, TIMEOUT_MS) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

/**
 * @brief Scan I2C bus for devices
 */
uint8_t I2C::scan(uint8_t* devices, uint8_t maxCount)
{
    uint8_t count = 0;

    // Scan addresses 0x01 to 0x7F (7-bit addresses)
    for (uint8_t addr = 1; addr < 128 && count < maxCount; addr++) {
        if (HAL_I2C_IsDeviceReady(m_hi2c, addr << 1, 1, 10) == HAL_OK) {
            devices[count++] = addr;
        }
    }

    return count;
}

/**
 * @brief Check if device is present
 */
bool I2C::isDeviceReady(uint8_t devAddr)
{
    return HAL_I2C_IsDeviceReady(m_hi2c, devAddr << 1, 1, 10) == HAL_OK;
}

/*============================================================================
 * C API Implementation
 *============================================================================*/

extern "C" {

e_I2C_Status e_I2C_WriteReg(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    uint8_t buf[2] = {regAddr, data};
    if (HAL_I2C_Master_Transmit(hi2c, devAddr << 1, buf, 2, 1000) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

e_I2C_Status e_I2C_ReadReg(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint8_t* data)
{
    if (HAL_I2C_Master_Transmit(hi2c, devAddr << 1, &regAddr, 1, 1000) != HAL_OK) {
        return e_I2C_Status::ERROR;
    }
    if (HAL_I2C_Master_Receive(hi2c, devAddr << 1, data, 1, 1000) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

e_I2C_Status e_I2C_WriteReg16(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint16_t data)
{
    uint8_t buf[3] = {regAddr, static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data & 0xFF)};
    if (HAL_I2C_Master_Transmit(hi2c, devAddr << 1, buf, 3, 1000) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

e_I2C_Status e_I2C_ReadReg16(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint16_t* data)
{
    uint8_t buf[2];
    if (HAL_I2C_Master_Transmit(hi2c, devAddr << 1, &regAddr, 1, 1000) != HAL_OK) {
        return e_I2C_Status::ERROR;
    }
    if (HAL_I2C_Master_Receive(hi2c, devAddr << 1, buf, 2, 1000) == HAL_OK) {
        *data = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

e_I2C_Status e_I2C_WriteRegs(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, const uint8_t* data, size_t len)
{
    // Use stack for small transfers, heap for larger
    if (len > 32) {
        return e_I2C_Status::ERROR;  // Limit stack usage
    }

    uint8_t buf[33];
    buf[0] = regAddr;
    for (size_t i = 0; i < len; i++) {
        buf[i + 1] = data[i];
    }

    if (HAL_I2C_Master_Transmit(hi2c, devAddr << 1, buf, len + 1, 1000) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

e_I2C_Status e_I2C_ReadRegs(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint8_t* data, size_t len)
{
    if (HAL_I2C_Master_Transmit(hi2c, devAddr << 1, &regAddr, 1, 1000) != HAL_OK) {
        return e_I2C_Status::ERROR;
    }
    if (HAL_I2C_Master_Receive(hi2c, devAddr << 1, data, len, 1000) == HAL_OK) {
        return e_I2C_Status::OK;
    }
    return e_I2C_Status::ERROR;
}

uint8_t e_I2C_Scan(I2C_HandleTypeDef* hi2c, uint8_t* devices, uint8_t maxCount)
{
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 128 && count < maxCount; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 1, 10) == HAL_OK) {
            devices[count++] = addr;
        }
    }
    return count;
}

bool b_I2C_IsDeviceReady(I2C_HandleTypeDef* hi2c, uint8_t devAddr)
{
    return HAL_I2C_IsDeviceReady(hi2c, devAddr << 1, 1, 10) == HAL_OK;
}

} // extern "C"