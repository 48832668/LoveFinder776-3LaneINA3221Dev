/**
 * @file main.cpp
 * @brief INA3221 Three-Channel Voltage/Current Monitor - C++17
 */

#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "adc.h"

#include "ST7735.hpp"   // includes fonts.hpp
#include "I2C.hpp"
#include "INA3221.hpp"
#include "BUTTON.hpp"
#include "EEPROM.hpp"

#include <cstdio>

/*============================================================================
 * Constants
 *============================================================================*/

// INA3221 address: A0=VS => 0x41
constexpr uint8_t INA3221_ADDR = 0x41;
// Shunt resistor: 0.01Ω (10mΩ)
constexpr float SHUNT_RESISTOR = 0.01f;

// Display layout (160x80 LCD)
namespace Display {
    // Column positions for 3-channel display
    constexpr uint8_t COL1_X = 3;    // CH1
    constexpr uint8_t COL2_X = 58;   // CH2
    constexpr uint8_t COL3_X = 113;  // CH3
    constexpr uint8_t COL_W  = 48;   // column width

    // Row Y positions (vertically centered in 80px)
    constexpr uint8_t LABEL_Y   = 7;     // Font_11x18: IN/OUT badge
    constexpr uint8_t VOLTAGE_Y = 27;    // Font_11x18: voltage
    constexpr uint8_t CURRENT_Y = 52;    // Font_11x18: current

    // Progress bar positions (just below each value)
    constexpr uint8_t V_BAR_Y   = VOLTAGE_Y + 18 + 1;  // 46
    constexpr uint8_t A_BAR_Y   = CURRENT_Y + 18 + 1;  // 71

    // Progress bar dimensions
    constexpr uint8_t BAR_W     = 42;    // bar width (matching reference)
    constexpr uint8_t BAR_H     = 3;     // bar height

    // Max values for percentage
    constexpr uint16_t V_MAX_mV = 26000;  // 26V
    constexpr int32_t  A_MAX_mA = 10000;  // 10A (user specified)

    // Progress bar animation
    constexpr uint8_t SEG_WIDTH  = 12;    // animated slider width (matching reference)
    constexpr uint8_t MAX_POS    = BAR_W - SEG_WIDTH;  // 30
    constexpr uint8_t ANIM_FRAMES = MAX_POS * 2;       // 60

    // Rounded badge for IN/OUT label (Font_11x18 = 18px, badge = 20px with 1px margin)
    constexpr uint8_t LABEL_BG_W  = 48;    // same as column width
    constexpr uint8_t LABEL_BG_H  = 20;    // font 18px + 2px margin
    constexpr uint8_t LABEL_BG_Y  = LABEL_Y - 1;  // 1px padding top, font 18px + 1px bottom
    constexpr uint8_t LABEL_X_PAD = 7;     // center "OUT"/"IN " (33px) in 48px badge
}

// Column X positions array for indexed access
constexpr uint8_t COL_X[3] = {
    Display::COL1_X,
    Display::COL2_X,
    Display::COL3_X
};

/*============================================================================
 * Global Variables
 *============================================================================*/

I2C i2cDriver(&hi2c1);
EEPROM eeprom;
INA3221 ina3221;
ST7735 lcd;

// Previous values for change detection
static uint16_t prevVoltage_mV[3] = {0xFFFF, 0xFFFF, 0xFFFF};
static int32_t  prevCurrent_mA[3] = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
static int8_t   prevDirection[3] = {0, 0, 0};  // 0=init, 1=OUT, -1=IN

// Animation state for idle bars
static uint8_t animCounter = 0;

/*============================================================================
 * Function Prototypes
 *============================================================================*/

void SystemClock_Config(void);
void InitPeripherals(void);
void DisplayInit(void);
void UpdateDisplay(const INA3221_ChannelData data[3]);

/*============================================================================
 * IN/OUT Badge Helper
 *============================================================================*/

// Draw SLATE rounded rectangle badge background (matching SW3526 reference style)
static void drawBadgeBg(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    constexpr uint8_t R = 3;
    lcd.fillRectangleFast(x, y, w, h, ST7735_Color::SLATE);
    // Top-left corner
    lcd.fillRectangleFast(x, y, R, 1, ST7735_Color::BLACK);
    lcd.fillRectangleFast(x, y + 1, 1, R - 1, ST7735_Color::BLACK);
    // Top-right corner
    lcd.fillRectangleFast(x + w - R, y, R, 1, ST7735_Color::BLACK);
    lcd.fillRectangleFast(x + w - 1, y + 1, 1, R - 1, ST7735_Color::BLACK);
    // Bottom-left corner
    lcd.fillRectangleFast(x, y + h - 1, R, 1, ST7735_Color::BLACK);
    lcd.fillRectangleFast(x, y + h - R + 1, 1, R - 1, ST7735_Color::BLACK);
    // Bottom-right corner
    lcd.fillRectangleFast(x + w - R, y + h - 1, R, 1, ST7735_Color::BLACK);
    lcd.fillRectangleFast(x + w - 1, y + h - R + 1, 1, R - 1, ST7735_Color::BLACK);
}

/*============================================================================
 * Display Functions
 *============================================================================*/

void DisplayInit(void)
{
    lcd.fillScreen(ST7735_Color::BLACK);

    // Draw SLATE rounded badge for "IN"/"OUT" row (all 3 columns)
    for (int i = 0; i < 3; i++) {
        drawBadgeBg(COL_X[i] - 2, Display::LABEL_BG_Y,
                    Display::LABEL_BG_W, Display::LABEL_BG_H);
    }

    // Draw progress bar backgrounds (static, done once)
    for (int i = 0; i < 3; i++) {
        lcd.fillRectangleFast(COL_X[i], Display::V_BAR_Y,
                              Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
        lcd.fillRectangleFast(COL_X[i], Display::A_BAR_Y,
                              Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
    }

    // Force first update to draw all values
    prevVoltage_mV[0] = prevVoltage_mV[1] = prevVoltage_mV[2] = 0xFFFF;
    prevCurrent_mA[0] = prevCurrent_mA[1] = prevCurrent_mA[2] = 0x7FFFFFFF;
    prevDirection[0] = prevDirection[1] = prevDirection[2] = 0;
}

void UpdateDisplay(const INA3221_ChannelData data[3])
{
    char buf[16];

    // Advance animation counter (ping-pong: 0→MAX_POS→0→MAX_POS...)
    animCounter = (animCounter + 1) % Display::ANIM_FRAMES;

    uint8_t animPos;
    if (animCounter <= Display::MAX_POS) {
        animPos = animCounter;                    // Moving right: 0 → MAX_POS
    } else {
        animPos = Display::ANIM_FRAMES - animCounter;  // Moving left: MAX_POS → 0
    }

    for (int i = 0; i < 3; i++)
    {
        const uint8_t colX = COL_X[i];
        const INA3221_ChannelData& ch = data[2 - i];  // display order: CH3, CH2, CH1
        const uint16_t v_mV = ch.busVoltage_mV;
        const int32_t  i_mA = ch.current_mA;

        // --- IN/OUT Badge (Font_11x18, based on current direction) ---
        {
            // Positive current = flowing OUT to load (bus → load)
            // Negative current = flowing IN from source (source → bus)
            const bool isIn = (i_mA < 0);
            const int8_t dir = isIn ? 1 : -1;

            if (dir != prevDirection[i])
            {
                // Centered in 48px badge: "IN " = 33px, left pad = (48-33)/2 ≈ 7
                // "OUT" = 33px, same pad = 7
                const char* label = isIn ? "IN " : "OUT";
                uint16_t color = isIn ? ST7735_Color::YELLOW : ST7735_Color::CYAN;
                lcd.writeString(colX + 7, Display::LABEL_Y,
                                label, Font_11x18,
                                color, ST7735_Color::SLATE);
                prevDirection[i] = dir;
            }
        }

        // --- Bus Voltage (Font_11x18, CYAN) ---
        if (v_mV != prevVoltage_mV[i])
        {
            uint16_t v = v_mV / 1000;
            uint16_t d = (v_mV % 1000) / 100;
            snprintf(buf, sizeof(buf), "%02d.%d", v, d);
            lcd.writeString(colX, Display::VOLTAGE_Y,
                            buf, Font_11x18,
                            ST7735_Color::CYAN, ST7735_Color::BLACK);
            prevVoltage_mV[i] = v_mV;
        }

        // --- Voltage Progress Bar (always redrawn for animation) ---
        {
            uint8_t w = (static_cast<uint32_t>(v_mV) * Display::BAR_W) / Display::V_MAX_mV;
            if (w > Display::BAR_W) w = Display::BAR_W;

            lcd.fillRectangleFast(colX, Display::V_BAR_Y,
                                  Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);

            if (v_mV < 1000) {
                // Idle: animated slider
                lcd.fillRectangleFast(colX + animPos, Display::V_BAR_Y,
                                      Display::SEG_WIDTH, Display::BAR_H, ST7735_Color::CYAN);
            } else if (w > 0) {
                // Active: proportional bar
                lcd.fillRectangleFast(colX, Display::V_BAR_Y,
                                      w, Display::BAR_H, ST7735_Color::CYAN);
            }
        }

        // --- Current (Font_11x18, YELLOW) ---
        if (i_mA != prevCurrent_mA[i])
        {
            uint32_t abs_mA = (i_mA >= 0) ? static_cast<uint32_t>(i_mA) : static_cast<uint32_t>(-i_mA);
            int a = abs_mA / 1000;
            int d1 = (abs_mA % 1000) / 100;
            int d2 = (abs_mA % 100) / 10;
            snprintf(buf, sizeof(buf), "%d.%d%d", a, d1, d2);
            lcd.writeString(colX, Display::CURRENT_Y,
                            buf, Font_11x18,
                            ST7735_Color::YELLOW, ST7735_Color::BLACK);
            prevCurrent_mA[i] = i_mA;
        }

        // --- Current Progress Bar (always redrawn for animation) ---
        {
            uint8_t w = (i_mA > 0) ? (static_cast<uint32_t>(i_mA) * Display::BAR_W) / Display::A_MAX_mA : 0;
            if (w > Display::BAR_W) w = Display::BAR_W;

            const bool idle = (i_mA <= 0 || v_mV < 1000);

            lcd.fillRectangleFast(colX, Display::A_BAR_Y,
                                  Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);

            if (idle) {
                // Idle: animated slider
                lcd.fillRectangleFast(colX + animPos, Display::A_BAR_Y,
                                      Display::SEG_WIDTH, Display::BAR_H, ST7735_Color::YELLOW);
            } else if (w > 0) {
                // Active: proportional bar
                lcd.fillRectangleFast(colX, Display::A_BAR_Y,
                                      w, Display::BAR_H, ST7735_Color::YELLOW);
            }
        }
    }
}

/*============================================================================
 * Main Function
 *============================================================================*/

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    InitPeripherals();

    // Quick LED test at startup - blink 3 times
    for (int i = 0; i < 3; i++)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
        HAL_Delay(200);
    }

    // Initialize ST7735 LCD (new C++ class API)
    lcd.init(&hspi1,
             LCD_CS_GPIO_Port, LCD_CS_Pin,
             LCD_DC_GPIO_Port, LCD_DC_Pin,
             LCD_RESET_GPIO_Port, LCD_RESET_Pin,
             LCD_EN_GPIO_Port, LCD_EN_Pin,
             "LCD_MAIN");
    lcd.begin();
    lcd.fillScreen(ST7735_Color::BLACK);

    // Initialize EEPROM (for I2C bus presence, not actively used)
    eeprom.init(&hi2c1);

    // Initialize INA3221
    if (!ina3221.init(&hi2c1, INA3221_ADDR, SHUNT_RESISTOR))
    {
        // INA3221 not found - show error
        lcd.writeString(30, 30, "INA3221 Not Found!", Font_7x10,
                        ST7735_Color::RED, ST7735_Color::BLACK);
        while (1) { HAL_Delay(1000); }
    }

    // Initialize Button (KEY1 on PA1, active low)
    BUTTON_Config btnConfig = BUTTON_Config::getDefault();
    Button keyButton(KEY1_GPIO_Port, KEY1_Pin, btnConfig);

    // Initialize display layout
    DisplayInit();

    // Show INA3221 found status briefly
    lcd.writeString(15, 68, "INA3221 Ready", Font_7x10,
                    ST7735_Color::GREEN, ST7735_Color::BLACK);
    HAL_Delay(500);
    // Clear status line
    lcd.fillRectangle(0, 68, 160, 10, ST7735_Color::BLACK);

    // Measurement data
    INA3221_ChannelData channelData[3];
    char mainBuf[32];

while (1)
{
    // Read all 3 channels
    ina3221.readAllChannels(channelData);

    // Update display (change detection only)
    UpdateDisplay(channelData);

    // Check button (re-scan I2C bus if pressed)
    keyButton.update();
    if (keyButton.getEvent() != e_BUTTON_Event::NONE)
    {
        // I2C re-scan placeholder - show device list briefly
        lcd.fillScreen(ST7735_Color::BLACK);
        lcd.writeString(30, 0, "I2C Devices", Font_7x10,
                        ST7735_Color::CYAN, ST7735_Color::BLACK);

        uint8_t devices[16];
        uint8_t count = i2cDriver.scan(devices, sizeof(devices));
        uint8_t y = 14;
        for (uint8_t j = 0; j < count; j++)
        {
            snprintf(mainBuf, sizeof(mainBuf), "0x%02X", devices[j]);
            lcd.writeString(5, y, mainBuf, Font_7x10,
                            ST7735_Color::WHITE, ST7735_Color::BLACK);
            y += 12;
        }
        HAL_Delay(2000);

        // Restore display
        DisplayInit();
    }

    HAL_Delay(200);  // 5 Hz update rate
}
}

/*============================================================================
 * System Clock Configuration
 *============================================================================*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 8;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/*============================================================================
 * Initialize Peripherals
 *============================================================================*/

void InitPeripherals(void)
{
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_TIM3_Init();
    MX_SPI1_Init();
    MX_ADC1_Init();
}

/*============================================================================
 * Error Handler
 *============================================================================*/

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

/*============================================================================
 * assert_failed
 *============================================================================*/

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif