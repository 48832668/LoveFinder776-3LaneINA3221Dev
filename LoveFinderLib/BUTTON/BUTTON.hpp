/**
 * @file BUTTON.hpp
 * @brief Button Driver - C++17
 * @author LoveFinder
 * @date 2026
 */

#ifndef BUTTON_HPP
#define BUTTON_HPP

#ifdef __cplusplus

#include "main.h"
#include <cstdint>

/*============================================================================
 * 常量定义 (constexpr)
 *============================================================================*/

namespace ButtonConfig {
    constexpr uint16_t DEBOUNCE_MS_DEFAULT     = 20;    // 消抖时间
    constexpr uint16_t LONG_PRESS_MS_DEFAULT   = 1000;  // 长按判定时间
    constexpr uint16_t DOUBLE_CLICK_MS_DEFAULT = 200;   // 双击间隔时间
}

/*============================================================================
 * 按键事件枚举 (enum class)
 *============================================================================*/

enum class e_BUTTON_Event : uint8_t {
    NONE        = 0,   // 无事件
    CLICK       = 1,   // 单击
    DOUBLE_CLICK= 2,   // 双击
    LONG_PRESS  = 3,   // 长按
    PRESS_DOWN  = 4,   // 按下瞬间
    RELEASE     = 5    // 释放
};

/*============================================================================
 * 按键状态枚举 (enum class) - 轮询模式
 *============================================================================*/

enum class e_BUTTON_State : uint8_t {
    IDLE        = 0,   // 空闲状态
    DEBOUNCE    = 1,   // 消抖状态
    PRESSED     = 2,   // 已按下
    WAIT_RELEASE= 3,   // 等待释放
    WAIT_CLICK  = 4    // 等待第二次点击
};

/*============================================================================
 * 按键配置结构体
 *============================================================================*/

struct BUTTON_Config {
    uint16_t debounceMs;      // 消抖时间 (ms)
    uint16_t longPressMs;     // 长按判定时间 (ms)
    uint16_t doubleClickMs;   // 双击间隔时间 (ms)
    bool activeLow;           // true: 低电平按下, false: 高电平按下

    // 默认配置工厂函数
    static BUTTON_Config getDefault() {
        return {
            ButtonConfig::DEBOUNCE_MS_DEFAULT,
            ButtonConfig::LONG_PRESS_MS_DEFAULT,
            ButtonConfig::DOUBLE_CLICK_MS_DEFAULT,
            true  // 默认低电平有效
        };
    }
};


/*============================================================================
 * 按键计数器结构体
 *============================================================================*/

struct BUTTON_Counter {
    uint16_t clickCount;       // 单击次数
    uint16_t doubleClickCount; // 双击次数
    uint16_t longPressCount;   // 长按次数
};


/*============================================================================
 * 按键类
 *============================================================================*/

class Button {
public:
    /**
     * @brief 默认构造函数
     */
    Button() = default;

    /**
     * @brief 构造并初始化（使用默认配置）
     * @param port GPIO端口
     * @param pin GPIO引脚
     */
    Button(GPIO_TypeDef* port, uint16_t pin);

    /**
     * @brief 构造并初始化
     * @param port GPIO端口
     * @param pin GPIO引脚
     * @param config 配置参数
     */
    Button(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config);

    /**
     * @brief 构造并初始化（便捷方式）
     * @param port GPIO端口
     * @param pin GPIO引脚
     * @param activeLow true=低电平触发(按下为低), false=高电平触发(按下为高)
     */
    Button(GPIO_TypeDef* port, uint16_t pin, bool activeLow);

    /**
     * @brief 初始化（可用于默认构造后初始化）
     * @param port GPIO端口
     * @param pin GPIO引脚
     * @param config 配置参数
     */
    void init(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config);

    /**
     * @brief 初始化（使用默认配置）
     * @param port GPIO端口
     * @param pin GPIO引脚
     */
    void init(GPIO_TypeDef* port, uint16_t pin);

    /**
     * @brief 更新按键状态 (需在主循环中定期调用，建议1-10ms)
     * @return 当前按键事件
     */
    e_BUTTON_Event update();

    // C API required methods
    bool hasEvent() const { return m_hasEvent; }
    e_BUTTON_Event getEvent();
    const BUTTON_Counter& getCounter() const { return m_counter; }
    void resetCounter();
    bool isPressed() const;

private:
    GPIO_TypeDef* m_port = nullptr;
    uint16_t m_pin = 0;

    BUTTON_Config m_config;
    e_BUTTON_State m_state = e_BUTTON_State::IDLE;

    uint32_t m_lastTick = 0;
    uint32_t m_pressTick = 0;

    bool m_waitingSecondClick = false;
    bool m_pendingClickValid = false;
    bool m_confirmedDoubleClick = false;

    BUTTON_Counter m_counter = {0, 0, 0};

    volatile e_BUTTON_Event m_lastEvent = e_BUTTON_Event::NONE;
    volatile bool m_hasEvent = false;
};

#endif // BUTTON_HPP

#endif // __cplusplus