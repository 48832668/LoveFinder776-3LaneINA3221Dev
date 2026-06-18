/**
 * @file INA3221.hpp
 * @brief INA3221 Three-Channel Voltage/Current Monitor Driver - C++17
 * @description INA3221 is a triple-channel, high-side current and voltage monitor
 *              with I2C interface. Measures bus voltage and shunt voltage for 3 channels.
 * @author LoveFinder
 * @date 2026
 */

#ifndef INA3221_HPP
#define INA3221_HPP

#ifdef __cplusplus

#include "main.h"
#include "i2c.h"
#include <cstdint>
#include <cstddef>

/*============================================================================
 * Constants
 *============================================================================*/

namespace INA3221Config {
    // I2C Address (7-bit) - depends on A0 pin
    // A0=GND => 0x40, A0=VS => 0x41, A0=SDA => 0x42, A0=SCL => 0x43
    // Default shunt resistor: 0.1Ω (100mΩ) for current calculation
    constexpr float DEFAULT_SHUNT_OHM = 0.1f;

    // Register addresses
    constexpr uint8_t REG_CONFIG       = 0x00;
    constexpr uint8_t REG_SHUNT_VOLT1  = 0x01;  // CH1 shunt voltage
    constexpr uint8_t REG_BUS_VOLT1    = 0x02;  // CH1 bus voltage
    constexpr uint8_t REG_SHUNT_VOLT2  = 0x03;  // CH2 shunt voltage
    constexpr uint8_t REG_BUS_VOLT2    = 0x04;  // CH2 bus voltage
    constexpr uint8_t REG_SHUNT_VOLT3  = 0x05;  // CH3 shunt voltage
    constexpr uint8_t REG_BUS_VOLT3    = 0x06;  // CH3 bus voltage
    constexpr uint8_t REG_MANUFACTURER = 0xFE;  // Manufacturer ID
    constexpr uint8_t REG_DIE_ID       = 0xFF;  // Die ID

    // Configuration register bits
    constexpr uint16_t CFG_RESET       = 0x8000;  // Reset bit (bit 15)
    constexpr uint16_t CFG_CH_EN_MASK  = 0x7000;  // Channel enable (bits 14-12)
    constexpr uint16_t CFG_CH_EN_ALL   = 0x7000;  // Enable all 3 channels

    // Averaging mode (bits 11-9)
    constexpr uint16_t CFG_AVG_1     = 0x0000;  // 1 sample
    constexpr uint16_t CFG_AVG_4     = 0x0200;  // 4 samples
    constexpr uint16_t CFG_AVG_16    = 0x0400;  // 16 samples
    constexpr uint16_t CFG_AVG_64    = 0x0600;  // 64 samples
    constexpr uint16_t CFG_AVG_128   = 0x0800;  // 128 samples
    constexpr uint16_t CFG_AVG_256   = 0x0A00;  // 256 samples
    constexpr uint16_t CFG_AVG_512   = 0x0C00;  // 512 samples
    constexpr uint16_t CFG_AVG_1024  = 0x0E00;  // 1024 samples

    // Conversion time: bus voltage (bits 8-6)
    constexpr uint16_t CFG_VBUSCT_140us   = 0x0000;
    constexpr uint16_t CFG_VBUSCT_204us   = 0x0040;
    constexpr uint16_t CFG_VBUSCT_332us   = 0x0080;
    constexpr uint16_t CFG_VBUSCT_588us   = 0x00C0;
    constexpr uint16_t CFG_VBUSCT_1_1ms   = 0x0100;
    constexpr uint16_t CFG_VBUSCT_2_1ms   = 0x0140;
    constexpr uint16_t CFG_VBUSCT_4_2ms   = 0x0180;
    constexpr uint16_t CFG_VBUSCT_8_3ms   = 0x01C0;

    // Conversion time: shunt voltage (bits 5-3)
    constexpr uint16_t CFG_VSHCT_140us   = 0x0000;
    constexpr uint16_t CFG_VSHCT_204us   = 0x0008;
    constexpr uint16_t CFG_VSHCT_332us   = 0x0010;
    constexpr uint16_t CFG_VSHCT_588us   = 0x0018;
    constexpr uint16_t CFG_VSHCT_1_1ms   = 0x0020;
    constexpr uint16_t CFG_VSHCT_2_1ms   = 0x0028;
    constexpr uint16_t CFG_VSHCT_4_2ms   = 0x0030;
    constexpr uint16_t CFG_VSHCT_8_3ms   = 0x0038;

    // Operating mode (bits 2-0)
    constexpr uint16_t CFG_MODE_POWERD = 0x0000;  // Power-down
    constexpr uint16_t CFG_MODE_TRIG   = 0x0003;  // Triggered (shunt+bus)
    constexpr uint16_t CFG_MODE_CONT   = 0x0007;  // Continuous (shunt+bus)
}

/*============================================================================
 * Channel Data Structure
 *============================================================================*/

struct INA3221_ChannelData {
    int16_t shuntVoltage_uV;   // Shunt voltage in microvolts (signed)
    uint16_t busVoltage_mV;     // Bus voltage in millivolts
    int32_t current_mA;         // Current in milliamps (signed, calculated from shunt)
};

/*============================================================================
 * INA3221 Class
 *============================================================================*/

class INA3221 {
public:
    /**
     * @brief Default constructor
     */
    INA3221() = default;

    /**
     * @brief Initialize with I2C handle and device address
     * @param hi2c Pointer to HAL I2C handle
     * @param addr 7-bit I2C device address (default 0x40 for A0=GND)
     * @param shuntOhm Shunt resistor value in ohms (default 0.1)
     * @return true = device found and initialized
     */
    bool init(I2C_HandleTypeDef* hi2c, uint8_t addr = 0x40, float shuntOhm = INA3221Config::DEFAULT_SHUNT_OHM);

    /**
     * @brief Check if device is connected
     * @return true = connected
     */
    bool isConnected();

    /**
     * @brief Read bus voltage for a channel (1-3)
     * @param channel Channel number (1, 2, or 3)
     * @return Bus voltage in millivolts, 0 on error
     */
    uint16_t readBusVoltage(uint8_t channel);

    /**
     * @brief Read shunt voltage for a channel (1-3)
     * @param channel Channel number (1, 2, or 3)
     * @return Shunt voltage in microvolts (signed), 0 on error
     */
    int16_t readShuntVoltage(uint8_t channel);

    /**
     * @brief Read all data for a channel
     * @param channel Channel number (1, 2, or 3)
     * @return ChannelData struct with voltage/current
     */
    INA3221_ChannelData readChannel(uint8_t channel);

    /**
     * @brief Read all 3 channels at once
     * @param data Array of 3 ChannelData structs
     */
    void readAllChannels(INA3221_ChannelData data[3]);

    /**
     * @brief Get manufacturer ID
     * @return Manufacturer ID (should be 0x5449 for TI)
     */
    uint16_t readManufacturerId();

    /**
     * @brief Get die ID
     * @return Die ID (should be 0x3220)
     */
    uint16_t readDieId();

    /**
     * @brief Set shunt resistor value for current calculation
     * @param shuntOhm Shunt value in ohms
     */
    void setShuntResistor(float shuntOhm) { m_shuntOhm = shuntOhm; }

    /**
     * @brief Write full configuration register value
     * @param config 16-bit config value
     * @return true = write succeeded
     */
    bool setConfig(uint16_t config);

    /**
     * @brief Get device address
     * @return 7-bit I2C address
     */
    uint8_t getAddress() const { return m_addr; }

private:
    I2C_HandleTypeDef* m_i2c = nullptr;
    uint8_t m_addr = 0x40;
    float m_shuntOhm = INA3221Config::DEFAULT_SHUNT_OHM;
};

/*============================================================================
 * C API Compatibility Layer
 *============================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

bool b_INA3221_Init(INA3221* dev, I2C_HandleTypeDef* hi2c, uint8_t addr, float shuntOhm);
bool b_INA3221_IsConnected(INA3221* dev);
uint16_t u16_INA3221_ReadBusVoltage(INA3221* dev, uint8_t channel);
int16_t s16_INA3221_ReadShuntVoltage(INA3221* dev, uint8_t channel);
void v_INA3221_ReadAllChannels(INA3221* dev, INA3221_ChannelData data[3]);

#ifdef __cplusplus
}
#endif

#endif // __cplusplus

#endif // INA3221_HPP
