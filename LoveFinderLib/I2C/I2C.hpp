/**
 * @file I2C.hpp
 * @brief Generic I2C Driver - C++17
 * @description Provides basic I2C operations for use by other IC driver libraries
 * @author LoveFinder
 * @date 2026
 */

#ifndef I2C_HPP
#define I2C_HPP

#ifdef __cplusplus

#include "main.h"
#include "i2c.h"
#include <cstdint>
#include <cstddef>

/*============================================================================
 * I2C Status Enum
 *============================================================================*/

enum class e_I2C_Status : uint8_t {
    OK = 0,
    ERROR = 1,
    BUSY = 2,
    TIMEOUT = 3,
    NAK = 4
};

// Backward compatibility
#define I2C_OK     e_I2C_Status::OK
#define I2C_ERROR  e_I2C_Status::ERROR
#define I2C_BUSY   e_I2C_Status::BUSY
#define I2C_TIMEOUT e_I2C_Status::TIMEOUT
#define I2C_NAK    e_I2C_Status::NAK

/*============================================================================
 * I2C Driver Class
 *============================================================================*/

class I2C {
public:
    /**
     * @brief Construct with existing HAL handle
     * @param hi2c Pointer to HAL I2C handle (e.g., &hi2c1)
     */
    explicit I2C(I2C_HandleTypeDef* hi2c) : m_hi2c(hi2c) {}

    // Disable copy
    I2C(const I2C&) = delete;
    I2C& operator=(const I2C&) = delete;

    /*========================================================================
     * Basic Write Operations
     *=======================================================================*/

    /**
     * @brief Write single byte to a register
     * @param devAddr Device I2C address (7-bit)
     * @param regAddr Register address
     * @param data Data to write
     * @return I2C status
     */
    e_I2C_Status writeReg(uint8_t devAddr, uint8_t regAddr, uint8_t data);

    /**
     * @brief Write word (16-bit) to consecutive registers
     * @param devAddr Device I2C address (7-bit)
     * @param regAddr Starting register address
     * @param data 16-bit data to write (MSB first)
     * @return I2C status
     */
    e_I2C_Status writeReg16(uint8_t devAddr, uint8_t regAddr, uint16_t data);

    /**
     * @brief Write multiple bytes
     * @param devAddr Device I2C address (7-bit)
     * @param regAddr Starting register address
     * @param data Pointer to data buffer
     * @param len Number of bytes to write
     * @return I2C status
     */
    e_I2C_Status writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* data, size_t len);

    /**
     * @brief Write to device memory (no register address)
     * @param devAddr Device I2C address (7-bit)
     * @param data Pointer to data buffer
     * @param len Number of bytes to write
     * @return I2C status
     */
    e_I2C_Status write(uint8_t devAddr, const uint8_t* data, size_t len);

    /*========================================================================
     * Basic Read Operations
     *=======================================================================*/

    /**
     * @brief Read single byte from a register
     * @param devAddr Device I2C address (7-bit)
     * @param regAddr Register address
     * @param data Pointer to store read data
     * @return I2C status
     */
    e_I2C_Status readReg(uint8_t devAddr, uint8_t regAddr, uint8_t* data);

    /**
     * @brief Read word (16-bit) from consecutive registers
     * @param devAddr Device I2C address (7-bit)
     * @param regAddr Starting register address
     * @param data Pointer to store 16-bit read data (MSB first)
     * @return I2C status
     */
    e_I2C_Status readReg16(uint8_t devAddr, uint8_t regAddr, uint16_t* data);

    /**
     * @brief Read multiple bytes
     * @param devAddr Device I2C address (7-bit)
     * @param regAddr Starting register address
     * @param data Pointer to data buffer
     * @param len Number of bytes to read
     * @return I2C status
     */
    e_I2C_Status readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* data, size_t len);

    /**
     * @brief Read from device memory (no register address)
     * @param devAddr Device I2C address (7-bit)
     * @param data Pointer to data buffer
     * @param len Number of bytes to read
     * @return I2C status
     */
    e_I2C_Status read(uint8_t devAddr, uint8_t* data, size_t len);

    /*========================================================================
     * Utility Functions
     *=======================================================================*/

    /**
     * @brief Scan I2C bus for devices
     * @param devices Array to store found device addresses (must be large enough)
     * @param maxCount Maximum number of devices to find
     * @return Number of devices found
     */
    uint8_t scan(uint8_t* devices, uint8_t maxCount);

    /**
     * @brief Check if a device is present on the bus
     * @param devAddr Device I2C address (7-bit)
     * @return true if device responds
     */
    bool isDeviceReady(uint8_t devAddr);

    /*========================================================================
     * C API (for use from C code)
     *=======================================================================*/

    // Raw HAL handle accessor
    I2C_HandleTypeDef* getHandle() { return m_hi2c; }

private:
    I2C_HandleTypeDef* m_hi2c;

    // Internal timeout in milliseconds
    static constexpr uint32_t TIMEOUT_MS = 1000;
};

/*============================================================================
 * C API Compatibility Layer (extern "C")
 *============================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write single byte (C API)
 */
e_I2C_Status e_I2C_WriteReg(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint8_t data);

/**
 * @brief Read single byte (C API)
 */
e_I2C_Status e_I2C_ReadReg(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint8_t* data);

/**
 * @brief Write word (16-bit) (C API)
 */
e_I2C_Status e_I2C_WriteReg16(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint16_t data);

/**
 * @brief Read word (16-bit) (C API)
 */
e_I2C_Status e_I2C_ReadReg16(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint16_t* data);

/**
 * @brief Write multiple bytes (C API)
 */
e_I2C_Status e_I2C_WriteRegs(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, const uint8_t* data, size_t len);

/**
 * @brief Read multiple bytes (C API)
 */
e_I2C_Status e_I2C_ReadRegs(I2C_HandleTypeDef* hi2c, uint8_t devAddr, uint8_t regAddr, uint8_t* data, size_t len);

/**
 * @brief Scan I2C bus (C API)
 */
uint8_t e_I2C_Scan(I2C_HandleTypeDef* hi2c, uint8_t* devices, uint8_t maxCount);

/**
 * @brief Check device ready (C API)
 */
bool b_I2C_IsDeviceReady(I2C_HandleTypeDef* hi2c, uint8_t devAddr);

#ifdef __cplusplus
}
#endif

#endif // __cplusplus

#endif // I2C_HPP