/**
 * @file INA3221.cpp
 * @brief INA3221 Three-Channel Voltage/Current Monitor Driver Implementation - C++17
 */

#include "INA3221.hpp"

/*============================================================================
 * Private Helpers
 *============================================================================*/

static uint8_t getShuntReg(uint8_t channel)
{
    switch (channel) {
        case 1:  return INA3221Config::REG_SHUNT_VOLT1;
        case 2:  return INA3221Config::REG_SHUNT_VOLT2;
        case 3:  return INA3221Config::REG_SHUNT_VOLT3;
        default: return INA3221Config::REG_SHUNT_VOLT1;
    }
}

static uint8_t getBusReg(uint8_t channel)
{
    switch (channel) {
        case 1:  return INA3221Config::REG_BUS_VOLT1;
        case 2:  return INA3221Config::REG_BUS_VOLT2;
        case 3:  return INA3221Config::REG_BUS_VOLT3;
        default: return INA3221Config::REG_BUS_VOLT1;
    }
}

/*============================================================================
 * Public Methods
 *============================================================================*/

bool INA3221::init(I2C_HandleTypeDef* hi2c, uint8_t addr, const ShuntConfig& shuntCfg)
{
    m_addr = addr;
    m_shuntCfg = shuntCfg;
    m_i2c = hi2c;

    // Configure: all channels enabled, 128-sample averaging,
    // 1.1ms bus CT, 1.1ms shunt CT, continuous shunt+bus
    uint16_t cfg = INA3221Config::CFG_CH_EN_ALL |
                   INA3221Config::CFG_AVG_128 |
                   INA3221Config::CFG_VBUSCT_1_1ms |
                   INA3221Config::CFG_VSHCT_1_1ms |
                   INA3221Config::CFG_MODE_CONT;
    return setConfig(cfg);
}

bool INA3221::setConfig(uint16_t config)
{
    if (m_i2c == nullptr) return false;

    uint8_t buf[3] = {INA3221Config::REG_CONFIG,
                      static_cast<uint8_t>(config >> 8),
                      static_cast<uint8_t>(config & 0xFF)};
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, buf, 3, 100) != HAL_OK) {
        return false;
    }

    // Verify readback
    uint8_t cfg[2];
    buf[0] = INA3221Config::REG_CONFIG;
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, buf, 1, 100) != HAL_OK) return false;
    if (HAL_I2C_Master_Receive(m_i2c, m_addr << 1, cfg, 2, 100) != HAL_OK) return false;

    uint16_t readback = (static_cast<uint16_t>(cfg[0]) << 8) | cfg[1];
    // Compare the readback to the value we wrote (mask off the RESET bit which
    // self-clears after a software reset)
    const uint16_t COMPARE_MASK = ~INA3221Config::CFG_RESET;
    return (readback & COMPARE_MASK) == (config & COMPARE_MASK);
}

bool INA3221::isConnected()
{
    if (m_i2c == nullptr) return false;
    return HAL_I2C_IsDeviceReady(m_i2c, m_addr << 1, 1, 10) == HAL_OK;
}

uint16_t INA3221::readBusVoltage(uint8_t channel)
{
    if (m_i2c == nullptr) return 0;

    uint8_t reg = getBusReg(channel);
    uint8_t buf[2] = {0, 0};

    // Write register address
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, &reg, 1, 100) != HAL_OK)
        return 0;

    // Read 2 bytes
    if (HAL_I2C_Master_Receive(m_i2c, m_addr << 1, buf, 2, 100) != HAL_OK)
        return 0;

    uint16_t raw = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    // Bus voltage: bits [14:3] valid data, LSB = 8 mV, bits [2:0] not used
    return (raw >> 3) * 8;
}

int32_t INA3221::readShuntVoltage(uint8_t channel)
{
    if (m_i2c == nullptr) return 0;
    if (channel < 1 || channel > 3) return 0;

    uint8_t reg = getShuntReg(channel);
    uint8_t buf[2] = {0, 0};

    // Write register address
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, &reg, 1, 100) != HAL_OK)
        return 0;

    // Read 2 bytes
    if (HAL_I2C_Master_Receive(m_i2c, m_addr << 1, buf, 2, 100) != HAL_OK)
        return 0;

    uint16_t raw = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    // INA3221 shunt voltage register: 12-bit signed data left-justified in
    // bits [14:3]; bits [2:0] unused (reads as 0). LSB = 40 µV.
    // sign_extend(reg >> 3, 12) gives the 12-bit signed value in µV*40 units.
    int32_t shunt_uV = (static_cast<int32_t>(static_cast<int16_t>(raw)) >> 3) * 40;

    return shunt_uV;
}

INA3221_ChannelData INA3221::readChannel(uint8_t channel)
{
    INA3221_ChannelData data = {0, 0, 0};
    if (m_i2c == nullptr) return data;
    if (channel < 1 || channel > 3) return data;

    uint8_t reg = getBusReg(channel);
    uint8_t buf[2];

    // Read bus voltage
    buf[0] = reg;
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, buf, 1, 100) != HAL_OK) return data;
    if (HAL_I2C_Master_Receive(m_i2c, m_addr << 1, buf, 2, 100) != HAL_OK) return data;
    uint16_t busRaw = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    data.busVoltage_mV = (busRaw >> 3) * 8;

    // Read shunt voltage (raw, no reversal applied yet)
    data.shuntVoltage_uV = readShuntVoltage(channel);

    // Apply per-channel IN+/IN- swap correction for current calculation only
    int32_t correctedShunt = data.shuntVoltage_uV;
    if (m_reverseDir[channel - 1]) {
        correctedShunt = -correctedShunt;
    }

    // Current = corrected shunt voltage / shunt resistance (per-channel)
    float shuntOhm = m_shuntCfg.get(channel);
    if (shuntOhm > 0.0001f) {
        float currentA = static_cast<float>(correctedShunt) / 1000000.0f / shuntOhm;
        data.current_mA = static_cast<int32_t>(currentA * 1000.0f);
    }

    return data;
}

void INA3221::setShuntResistor(uint8_t channel, float shuntOhm)
{
    if (channel < 1 || channel > 3) return;
    switch (channel) {
        case 1: m_shuntCfg.ch1 = shuntOhm; break;
        case 2: m_shuntCfg.ch2 = shuntOhm; break;
        case 3: m_shuntCfg.ch3 = shuntOhm; break;
        default: break;
    }
}

void INA3221::setChannelDirectionReversed(uint8_t channel, bool reversed)
{
    if (channel < 1 || channel > 3) return;
    m_reverseDir[channel - 1] = reversed;
}

bool INA3221::isChannelDirectionReversed(uint8_t channel) const
{
    if (channel < 1 || channel > 3) return false;
    return m_reverseDir[channel - 1];
}

INA3221_Direction INA3221::getChannelDirection(uint8_t channel)
{
    if (channel < 1 || channel > 3) return INA3221_Direction::OUT;
    int32_t shunt = readShuntVoltage(channel);
    // Apply per-channel IN+/IN- swap correction (same as readChannel)
    if (m_reverseDir[channel - 1]) {
        shunt = -shunt;
    }
    return getDirection(shunt);
}

void INA3221::readAllChannels(INA3221_ChannelData data[3])
{
    for (uint8_t ch = 1; ch <= 3; ch++) {
        data[ch - 1] = readChannel(ch);
    }
}

uint16_t INA3221::readManufacturerId()
{
    if (m_i2c == nullptr) return 0;
    uint8_t reg = INA3221Config::REG_MANUFACTURER;
    uint8_t buf[2] = {0, 0};
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, &reg, 1, 100) != HAL_OK) return 0;
    if (HAL_I2C_Master_Receive(m_i2c, m_addr << 1, buf, 2, 100) != HAL_OK) return 0;
    return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
}

uint16_t INA3221::readDieId()
{
    if (m_i2c == nullptr) return 0;
    uint8_t reg = INA3221Config::REG_DIE_ID;
    uint8_t buf[2] = {0, 0};
    if (HAL_I2C_Master_Transmit(m_i2c, m_addr << 1, &reg, 1, 100) != HAL_OK) return 0;
    if (HAL_I2C_Master_Receive(m_i2c, m_addr << 1, buf, 2, 100) != HAL_OK) return 0;
    return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
}

/*============================================================================
 * C API Implementation
 *============================================================================*/

extern "C" {

bool b_INA3221_Init(INA3221* dev, I2C_HandleTypeDef* hi2c, uint8_t addr, const ShuntConfig& shuntCfg)
{
    return dev->init(hi2c, addr, shuntCfg);
}

bool b_INA3221_IsConnected(INA3221* dev)
{
    return dev->isConnected();
}

uint16_t u16_INA3221_ReadBusVoltage(INA3221* dev, uint8_t channel)
{
    return dev->readBusVoltage(channel);
}

int32_t s32_INA3221_ReadShuntVoltage(INA3221* dev, uint8_t channel)
{
    return dev->readShuntVoltage(channel);
}

void v_INA3221_ReadAllChannels(INA3221* dev, INA3221_ChannelData data[3])
{
    dev->readAllChannels(data);
}

INA3221_Direction e_INA3221_GetChannelDirection(INA3221* dev, uint8_t channel)
{
    return dev->getChannelDirection(channel);
}

} // extern "C"