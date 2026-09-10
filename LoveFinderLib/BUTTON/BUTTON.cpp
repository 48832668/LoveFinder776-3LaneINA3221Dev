/**
 * @file BUTTON.cpp
 * @brief Button Driver Implementation - C++17
 */

#include "BUTTON.hpp"

/*============================================================================
 * Button 类实现
 *============================================================================*/

Button::Button(GPIO_TypeDef* port, uint16_t pin)
{
    m_port = port;
    m_pin = pin;
    m_config = BUTTON_Config::getDefault();
    m_state = e_BUTTON_State::IDLE;
    m_lastTick = 0;
    m_pressTick = 0;
    m_waitingSecondClick = false;
    m_pendingClickValid = false;
    m_confirmedDoubleClick = false;
    m_counter = {0, 0, 0};
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
}

Button::Button(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config)
{
    m_port = port;
    m_pin = pin;
    m_config = config;
    m_state = e_BUTTON_State::IDLE;
    m_lastTick = 0;
    m_pressTick = 0;
    m_waitingSecondClick = false;
    m_pendingClickValid = false;
    m_confirmedDoubleClick = false;
    m_counter = {0, 0, 0};
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
}

Button::Button(GPIO_TypeDef* port, uint16_t pin, bool activeLow)
{
    m_port = port;
    m_pin = pin;
    m_config = BUTTON_Config::getDefault();
    m_config.activeLow = activeLow;
    m_state = e_BUTTON_State::IDLE;
    m_lastTick = 0;
    m_pressTick = 0;
    m_waitingSecondClick = false;
    m_pendingClickValid = false;
    m_confirmedDoubleClick = false;
    m_counter = {0, 0, 0};
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
}

void Button::init(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config)
{
    m_port = port;
    m_pin = pin;
    m_config = config;
    m_state = e_BUTTON_State::IDLE;
    m_lastTick = 0;
    m_pressTick = 0;
    m_waitingSecondClick = false;
    m_pendingClickValid = false;
    m_confirmedDoubleClick = false;
    m_counter = {0, 0, 0};
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
}

void Button::init(GPIO_TypeDef* port, uint16_t pin)
{
    init(port, pin, BUTTON_Config::getDefault());
}

e_BUTTON_Event Button::update()
{
    uint32_t currentTick = HAL_GetTick();
    bool physicalPressed = isPressed();
    e_BUTTON_Event event = e_BUTTON_Event::NONE;

    switch (m_state)
    {
        case e_BUTTON_State::IDLE:
            if (physicalPressed)
            {
                m_state = e_BUTTON_State::DEBOUNCE;
                m_lastTick = currentTick;
            }
            break;

        case e_BUTTON_State::DEBOUNCE:
            if ((currentTick - m_lastTick) >= m_config.debounceMs)
            {
                if (physicalPressed)
                {
                    m_state = e_BUTTON_State::PRESSED;
                    m_pressTick = currentTick;
                    event = e_BUTTON_Event::PRESS_DOWN;
                    m_lastEvent = event;
                    m_hasEvent = true;
                }
                else
                {
                    m_state = e_BUTTON_State::IDLE;
                }
            }
            break;

        case e_BUTTON_State::PRESSED:
            if (!physicalPressed)
            {
                // 释放
                if ((currentTick - m_pressTick) >= m_config.longPressMs)
                {
                    // 长按释放
                    m_counter.longPressCount++;
                    event = e_BUTTON_Event::LONG_PRESS;
                    m_lastEvent = event;
                    m_hasEvent = true;
                    m_confirmedDoubleClick = false;
                    m_pendingClickValid = false;
                    m_waitingSecondClick = false;  // 清除双击标记，防止长按后再次按下时误判
                    m_state = e_BUTTON_State::IDLE;
                }
                else
                {
                    // 短按释放，检查是否已经是确认的双击
                    if (m_confirmedDoubleClick)
                    {
                        // 已经是双击的第二次释放，直接回到IDLE
                        m_confirmedDoubleClick = false;
                        m_pendingClickValid = false;
                        m_waitingSecondClick = false;
                        m_state = e_BUTTON_State::IDLE;
                    }
                    else
                    {
                        // 第一次短按释放，进入等待双击确认状态
                        m_state = e_BUTTON_State::WAIT_CLICK;
                        m_lastTick = currentTick;
                        m_pendingClickValid = true;
                    }
                }
            }
            else if ((currentTick - m_pressTick) >= m_config.longPressMs)
            {
                // 长按触发 (还在按着)
            }
            break;

        case e_BUTTON_State::WAIT_CLICK:
            if (physicalPressed)
            {
                // 第二次按下 - 双击确认
                if (m_pendingClickValid)
                {
                    // 确认双击
                    m_counter.doubleClickCount++;
                    m_pendingClickValid = false;
                    m_confirmedDoubleClick = true;

                    event = e_BUTTON_Event::DOUBLE_CLICK;
                    m_lastEvent = event;
                    m_hasEvent = true;
                }
                m_waitingSecondClick = true;
                // 重置计时起点，防止双击的第二次按下被误判为长按
                m_pressTick = currentTick;
                m_state = e_BUTTON_State::PRESSED;
            }
            else if ((currentTick - m_lastTick) >= m_config.doubleClickMs)
            {
                // 超时，确认单击
                if (m_pendingClickValid)
                {
                    m_counter.clickCount++;
                    event = e_BUTTON_Event::CLICK;
                    m_lastEvent = event;
                    m_hasEvent = true;
                    m_pendingClickValid = false;
                }
                m_state = e_BUTTON_State::IDLE;
            }
            break;

        default:
            m_state = e_BUTTON_State::IDLE;
            break;
    }

    return event;
}

e_BUTTON_Event Button::getEvent()
{
    e_BUTTON_Event event = m_lastEvent;
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
    return event;
}

void Button::resetCounter()
{
    m_counter = {0, 0, 0};
}

bool Button::isPressed() const
{
    GPIO_PinState state = HAL_GPIO_ReadPin(m_port, m_pin);
    return m_config.activeLow ? (state == GPIO_PIN_RESET) : (state == GPIO_PIN_SET);
}
