/**
 * @file BUTTON.cpp
 * @brief Button Driver Implementation - C++17
 */

#include "BUTTON.hpp"

/*============================================================================
 * 定时器相关变量
 *============================================================================*/

// 定时器句柄（静态变量）
static TIM_HandleTypeDef* s_htim = nullptr;

// 所有按钮实例的指针数组（支持多按钮）
static Button* s_buttons[4] = {nullptr, nullptr, nullptr, nullptr};
static uint8_t s_buttonCount = 0;

/*============================================================================
 * 定时器相关实现
 *============================================================================*/

/**
 * @brief 设置定时器用于长按检测
 */
void v_BUTTON_SetTimer(TIM_HandleTypeDef* htim)
{
    s_htim = htim;
}

/**
 * @brief 注册按钮到定时器系统
 */
void Button::registerToTimer()
{
    if (s_buttonCount < 4)
    {
        s_buttons[s_buttonCount++] = this;
    }
}

/**
 * @brief 定时器中断回调（在TIMx_IRQHandler中调用）
 */
extern "C" void v_BUTTON_TimerCallback(TIM_HandleTypeDef* htim)
{
    if (htim == s_htim)
    {
        // 更新所有已注册的按钮
        for (uint8_t i = 0; i < s_buttonCount; i++)
        {
            if (s_buttons[i])
            {
                s_buttons[i]->onTimerTick();
            }
        }
    }
}

/**
 * @brief 定时器 tick 处理
 */
void Button::onTimerTick()
{
    // 只在按下状态时计数
    if (m_isPressed)
    {
        m_pressDuration++;

        // 检查是否达到长按阈值
        if (m_pressDuration >= m_config.longPressMs)
        {
            // 触发长按事件（仅触发一次）
            if (!m_longPressTriggered)
            {
                m_longPressTriggered = true;
                m_counter.longPressCount++;
                m_lastEvent = e_BUTTON_Event::LONG_PRESS;
                m_hasEvent = true;
            }
        }
    }
}

/*============================================================================
 * Button 类实现
 *============================================================================*/

Button::Button(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config)
{
    init(port, pin, config);
}

Button::Button(GPIO_TypeDef* port, uint16_t pin, bool activeLow)
{
    BUTTON_Config config = BUTTON_Config::getDefault();
    config.activeLow = activeLow;
    init(port, pin, config);
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

    // 初始化长按检测变量
    m_isPressed = false;
    m_pressDuration = 0;
    m_longPressTriggered = false;

    // EXTI模式变量
    m_extiState = e_EXTI_State::IDLE;
    m_pressCount = 0;

    m_counter = {0, 0, 0};
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;

    // 注册到定时器系统
    registerToTimer();
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
                    m_state = e_BUTTON_State::IDLE;
                }
                else
                {
                    // 短按释放，检查是否已经是确认的双击
                    if (m_confirmedDoubleClick)
                    {
                        // 已经是双击的第二次释放，直接回到IDLE
                        m_confirmedDoubleClick = false;
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
                m_state = e_BUTTON_State::PRESSED;
                m_pressTick = currentTick;
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

/**
 * @brief EXTI触发处理（基于时间戳，无需轮询）
 * 在EXTI下降沿中断中调用（按键按下时触发）
 */
void Button::extiTrigger()
{
    uint32_t currentTick = HAL_GetTick();

    // 记录按下状态（用于长按检测）
    m_isPressed = true;
    m_pressDuration = 0;
    m_longPressTriggered = false;

    switch (m_extiState)
    {
        case e_EXTI_State::IDLE:
            // 第一次按下
            m_extiState = e_EXTI_State::WAIT;
            m_lastTick = currentTick;
            m_pressCount = 1;
            break;

        case e_EXTI_State::WAIT:
            // 再次按下（双击）
            if ((currentTick - m_lastTick) <= m_config.doubleClickMs)
            {
                m_pressCount++;
                m_lastTick = currentTick;
            }
            else
            {
                // 超时，重新开始
                m_extiState = e_EXTI_State::WAIT;
                m_lastTick = currentTick;
                m_pressCount = 1;
            }
            break;
    }
}

/**
 * @brief 按键释放处理（在主循环中检测释放）
 */
void Button::release()
{
    // 如果长按已触发，重置状态等待下次按下
    if (m_longPressTriggered)
    {
        m_longPressTriggered = false;
        m_pressDuration = 0;
        m_extiState = e_EXTI_State::IDLE;
        m_pressCount = 0;
        return;
    }

    // 如果在等待双击确认状态，释放意味着可能是单击（等待超时确认）
    // 状态保持不变，等待超时后确认
}

/**
 * @brief 检查EXTI事件（在主循环中调用，检查超时并返回事件）
 * @return 事件类型，无事件返回NONE
 * @note 应该在主循环中定期调用此方法检查超时和处理释放
 */
e_BUTTON_Event Button::checkExtiEvent()
{
    uint32_t currentTick = HAL_GetTick();
    bool currentPressed = isPressed();

    // 检测释放（从按下变为松开）
    if (m_isPressed && !currentPressed)
    {
        // 按键释放
        release();
    }

    // 更新当前按下状态
    m_isPressed = currentPressed;

    if (m_extiState == e_EXTI_State::WAIT)
    {
        // 检查是否超时（确认单击或双击）
        if ((currentTick - m_lastTick) > m_config.doubleClickMs)
        {
            e_BUTTON_Event event;

            if (m_pressCount >= 2)
            {
                // 双击
                m_counter.doubleClickCount++;
                event = e_BUTTON_Event::DOUBLE_CLICK;
            }
            else
            {
                // 单击
                m_counter.clickCount++;
                event = e_BUTTON_Event::CLICK;
            }

            m_lastEvent = event;
            m_hasEvent = true;
            m_extiState = e_EXTI_State::IDLE;
            m_pressCount = 0;

            return event;
        }
    }

return e_BUTTON_Event::NONE;
}

/*============================================================================
 * C API 兼容层
 *============================================================================*/

extern "C" {

BUTTON_Config BUTTON_GetDefaultConfig(void)
{
    return BUTTON_Config::getDefault();
}

void v_BUTTON_Init(BUTTON* btn, GPIO_TypeDef* port, uint16_t pin, BUTTON_Config* config)
{
    if (config)
    {
        btn->init(port, pin, *config);
    }
    else
    {
        btn->init(port, pin);
    }
}

e_BUTTON_Event e_BUTTON_Update(BUTTON* btn)
{
    return btn->update();
}

bool b_BUTTON_HasEvent(BUTTON* btn)
{
    return btn->hasEvent();
}

e_BUTTON_Event e_BUTTON_GetEvent(BUTTON* btn)
{
    return btn->getEvent();
}

const BUTTON_Counter* p_BUTTON_GetCounter(BUTTON* btn)
{
    return &btn->getCounter();
}

void v_BUTTON_ResetCounter(BUTTON* btn)
{
    btn->resetCounter();
}

bool b_BUTTON_IsPin(BUTTON* btn, uint16_t pin)
{
    return btn->isPin(pin);
}

bool b_BUTTON_IsPressed(BUTTON* btn)
{
    return btn->isPressed();
}

} // extern "C"
