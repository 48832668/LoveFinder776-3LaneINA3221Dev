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

// 200ms tick from TIM3 (declared in tim.c)
extern volatile uint8_t tim3_tick_flag;

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

    // Row Y positions (shifted +4px to make room for status bar at Y=0)
    constexpr uint8_t STATUS_Y   = 0;     // Font_7x10: power & charge
    constexpr uint8_t LABEL_Y   = 11;     // Font_11x18: IN/OUT badge (was 7)
    constexpr uint8_t VOLTAGE_Y = 31;    // Font_11x18: voltage (was 27)
    constexpr uint8_t CURRENT_Y = 56;    // Font_11x18: current (was 52)

    // Progress bar positions (just below each value)
    constexpr uint8_t V_BAR_Y   = VOLTAGE_Y + 18 + 1;  // 50 (was 46)
    constexpr uint8_t A_BAR_Y   = CURRENT_Y + 18 + 1;  // 75 (was 71)

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
 * Color Gradient Helpers
 *============================================================================*/

// Linear interpolate between two RGB565 colors in 5/6/5 space
// t: 0-255, t=0 → c1, t=255 → c2
static uint16_t lerpColor565(uint16_t c1, uint16_t c2, uint8_t t) {
    uint8_t r1 = (c1 >> 11) & 0x1F;
    uint8_t g1 = (c1 >> 5)  & 0x3F;
    uint8_t b1 =  c1        & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F;
    uint8_t g2 = (c2 >> 5)  & 0x3F;
    uint8_t b2 =  c2        & 0x1F;

    uint8_t r = r1 + ((static_cast<uint16_t>(r2 - r1) * t) >> 8);
    uint8_t g = g1 + ((static_cast<uint16_t>(g2 - g1) * t) >> 8);
    uint8_t b = b1 + ((static_cast<uint16_t>(b2 - b1) * t) >> 8);

    return (static_cast<uint16_t>(r) << 11) | (static_cast<uint16_t>(g) << 5) | b;
}

// Voltage color: green(0V) → yellow(5V) → orange(12V) → red(26V)
static uint16_t voltageColor(uint16_t mv) {
    if (mv < 5000) {
        uint8_t t = (mv * 255) / 5000;          // 0→5V: GREEN → YELLOW
        return lerpColor565(ST7735_Color::GREEN, ST7735_Color::YELLOW, t);
    } else if (mv < 12000) {
        uint8_t t = ((mv - 5000) * 255) / 7000; // 5→12V: YELLOW → ORANGE
        return lerpColor565(ST7735_Color::YELLOW, ST7735_Color::ORANGE, t);
    } else {
        uint8_t t = ((mv - 12000) * 255) / 14000; // 12→26V: ORANGE → RED
        if (t > 255) t = 255;
        return lerpColor565(ST7735_Color::ORANGE, ST7735_Color::RED, t);
    }
}

// Current color: directional palettes
// OUT: dim cyan(0A) → bright cyan → bright blue(10A)
// IN : warm yellow(0A) → orange → RED(10A)
static uint16_t currentColor(int32_t mA, bool isIn) {
    uint32_t abs_mA = (mA >= 0) ? static_cast<uint32_t>(mA) : static_cast<uint32_t>(-mA);
    if (abs_mA > 10000) abs_mA = 10000;
    uint8_t t = (abs_mA * 255) / 10000;

    if (isIn) {
        // IN: dim warm yellow → orange → red
        constexpr uint16_t DIM_WARM = ST7735_Color::RGB565(180, 120, 0);   // low current IN
        if (abs_mA < 5000) {
            uint8_t t2 = (abs_mA * 255) / 5000;
            return lerpColor565(DIM_WARM, ST7735_Color::ORANGE, t2);
        } else {
            uint8_t t2 = ((abs_mA - 5000) * 255) / 5000;
            return lerpColor565(ST7735_Color::ORANGE, ST7735_Color::RED, t2);
        }
    } else {
        // OUT: dim cyan → bright cyan → blue
        constexpr uint16_t DIM_CYAN = ST7735_Color::RGB565(60, 160, 180);  // low current OUT
        if (abs_mA < 5000) {
            uint8_t t2 = (abs_mA * 255) / 5000;
            return lerpColor565(DIM_CYAN, ST7735_Color::CYAN, t2);
        } else {
            uint8_t t2 = ((abs_mA - 5000) * 255) / 5000;
            return lerpColor565(ST7735_Color::CYAN, ST7735_Color::BRIGHT_BLUE, t2);
        }
    }
}

// Bar fill color uses the same gradient as text color
static uint16_t voltageBarColor(uint16_t mv) {
    return voltageColor(mv);
}

static uint16_t currentBarColor(int32_t mA, bool isIn) {
    return currentColor(mA, isIn);
}

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
static bool     prevIsIn[3] = {false, false, false};  // for IN centering switch
static uint8_t  prevVBarW[3] = {0xFF, 0xFF, 0xFF};    // previous bar widths
static uint8_t  prevABarW[3] = {0xFF, 0xFF, 0xFF};

// Animation state for idle bars
static uint8_t animCounter = 0;

// Power & charge accumulation (only count IN direction)
static uint32_t charge_mAs = 0;           // accumulated millianp-seconds
static uint16_t chargeFraction = 0;       // sub-mAs fraction (0.1 mAs units), avoids ÷5 rounding loss
static uint16_t prevPowerW_display = 0xFFFF;   // for dirty detect: W*10
static uint32_t prevCharge_mAh = 0xFFFFFFFF;   // for dirty detect

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

    // Clear status bar area (top 10px)
    lcd.fillRectangleFast(0, Display::STATUS_Y, 160, 10, ST7735_Color::BLACK);

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
    prevIsIn[0] = prevIsIn[1] = prevIsIn[2] = false;
    prevVBarW[0] = prevVBarW[1] = prevVBarW[2] = 0xFF;
    prevABarW[0] = prevABarW[1] = prevABarW[2] = 0xFF;
    prevPowerW_display = 0xFFFF;
    prevCharge_mAh = 0xFFFFFFFF;
    charge_mAs = 0;
    chargeFraction = 0;
}

void UpdateDisplay(const INA3221_ChannelData data[3])
{
    char buf[16];

    // Advance animation counter (ping-pong: 0→MAX_POS→0→MAX_POS...)
    animCounter = (animCounter + 1) % Display::ANIM_FRAMES;

    uint8_t animPos;
    if (animCounter <= Display::MAX_POS) {
        animPos = animCounter;
    } else {
        animPos = Display::ANIM_FRAMES - animCounter;
    }

    // --- Calculate total power & accumulate charge (OUT direction only) ---
    {
        uint32_t totalW_x10 = 0;  // W * 10, e.g. 1145 = 114.5W

        for (int i = 0; i < 3; i++) {
            const INA3221_ChannelData& ch = data[2 - i];
            const INA3221_Direction dir = INA3221::getDirection(ch.shuntVoltage_uV);
            if (dir == INA3221_Direction::IN) {
                // power = V * |I| / 1000 (mW)
                uint32_t abs_mA = (ch.current_mA >= 0) ? static_cast<uint32_t>(ch.current_mA)
                                                        : static_cast<uint32_t>(-ch.current_mA);
                totalW_x10 += (static_cast<uint32_t>(ch.busVoltage_mV) * abs_mA) / 100000;

                // charge: fractional accumulator avoids integer rounding loss
                // 200ms per tick → abs_mA * 200 = mAs in 0.1 mAs resolution
                chargeFraction += static_cast<uint16_t>(abs_mA * 2u);
                while (chargeFraction >= 10u) {
                    charge_mAs += 1;
                    chargeFraction -= 10u;
                }
            }
        }

        // Clamp totalW_x10 to 9999 (999.9W max display)
        if (totalW_x10 > 9999) totalW_x10 = 9999;
        // Clamp charge to prevent uint32_t overflow (~49 days at 10A on 1 channel)
        if (charge_mAs > 4280000000UL) charge_mAs = 4280000000UL;

        // --- Status Bar: Top line (Font_7x10) ---
        // Power format: WWW.YW (6 chars, 42px)
        {
            uint16_t w_int  = static_cast<uint16_t>(totalW_x10 / 10);
            uint8_t  w_dec  = static_cast<uint8_t>(totalW_x10 % 10);
            if (w_int != prevPowerW_display) {
                snprintf(buf, sizeof(buf), "%3u.%uW", w_int, w_dec);
                lcd.writeString(0, Display::STATUS_Y, buf, Font_7x10,
                                ST7735_Color::WHITE, ST7735_Color::BLACK);
                prevPowerW_display = w_int;
            }
        }

        // Charge format: XXXXXmAh (up to 7 chars, fits in remaining space)
        {
            uint32_t mAh = charge_mAs / 3600;
            if (mAh != prevCharge_mAh) {
                snprintf(buf, sizeof(buf), "%05umAh", mAh);
                lcd.writeString(104, Display::STATUS_Y, buf, Font_7x10,
                                ST7735_Color::RGB565(180, 220, 100), ST7735_Color::BLACK);
                prevCharge_mAh = mAh;
            }
        }
    }

    for (int i = 0; i < 3; i++)
    {
        const uint8_t colX = COL_X[i];
        const INA3221_ChannelData& ch = data[2 - i];  // display order: CH3, CH2, CH1
        const uint16_t v_mV = ch.busVoltage_mV;
        const int32_t  i_mA = ch.current_mA;
        const INA3221_Direction dir = INA3221::getDirection(ch.shuntVoltage_uV);
        const bool isIn = (dir == INA3221_Direction::IN);
        const int8_t dirVal = static_cast<int8_t>(dir);

        // --- IN/OUT Badge (Font_11x18, stable directional colors) ---
        if (dirVal != prevDirection[i])
        {
            const char* label = isIn ? "IN " : "OUT";
            uint16_t badgeColor = isIn ? ST7735_Color::RGB565(230, 140, 30)
                                       : ST7735_Color::CYAN;

            // Center "IN " or "OUT" (both 33px) in 48px badge
            lcd.writeString(colX + 7, Display::LABEL_Y,
                            label, Font_11x18,
                            badgeColor, ST7735_Color::SLATE);
            prevDirection[i] = dirVal;
        }

        // --- Bus Voltage (Font_11x18, gradient color) ---
        if (v_mV != prevVoltage_mV[i])
        {
            uint16_t v = v_mV / 1000;
            uint16_t d = (v_mV % 1000) / 100;
            snprintf(buf, sizeof(buf), "%02d.%d", v, d);
            uint16_t vColor = voltageColor(v_mV);
            lcd.writeString(colX, Display::VOLTAGE_Y,
                            buf, Font_11x18,
                            vColor, ST7735_Color::BLACK);
            prevVoltage_mV[i] = v_mV;
        }

        // --- Voltage Progress Bar (dirty-rect: only refill when width changes or idle) ---
        {
            uint8_t w = (static_cast<uint32_t>(v_mV) * Display::BAR_W) / Display::V_MAX_mV;
            if (w > Display::BAR_W) w = Display::BAR_W;

            const bool vIdle = (v_mV < 1000);

            if (vIdle || w != prevVBarW[i]) {
                lcd.fillRectangleFast(colX, Display::V_BAR_Y,
                                      Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
                if (vIdle) {
                    lcd.fillRectangleFast(colX + animPos, Display::V_BAR_Y,
                                          Display::SEG_WIDTH, Display::BAR_H,
                                          voltageBarColor(v_mV));
                } else if (w > 0) {
                    lcd.fillRectangleFast(colX, Display::V_BAR_Y,
                                          w, Display::BAR_H,
                                          voltageBarColor(v_mV));
                }
                prevVBarW[i] = w;
            } else if (w > 0 && w == prevVBarW[i] && v_mV >= 1000) {
                // Width unchanged and active — no redraw needed
            }
        }

        // --- Current (Font_11x18, gradient color, IN centered) ---
        if (i_mA != prevCurrent_mA[i] || isIn != prevIsIn[i])
        {
            uint32_t abs_mA = (i_mA >= 0) ? static_cast<uint32_t>(i_mA) : static_cast<uint32_t>(-i_mA);
            int a = abs_mA / 1000;
            int d1 = (abs_mA % 1000) / 100;
            int d2 = (abs_mA % 100) / 10;
            snprintf(buf, sizeof(buf), "%d.%d%d", a, d1, d2);

            uint16_t cColor = currentColor(i_mA, isIn);

            // IN mode: center text in column (makes visual distinction)
            uint8_t curX = colX;
            if (isIn) {
                // 4 chars = 44px, 5 chars = 55px; column = 48px
                uint8_t charCount = (abs_mA >= 10000) ? 5 : 4;
                int16_t textW = static_cast<int16_t>(charCount) * 11;
                int16_t pad = (static_cast<int16_t>(Display::COL_W) - textW) / 2;
                if (pad > 0) curX = colX + static_cast<uint8_t>(pad);
            }

            lcd.writeString(curX, Display::CURRENT_Y,
                            buf, Font_11x18,
                            cColor, ST7735_Color::BLACK);
            prevCurrent_mA[i] = i_mA;
            prevIsIn[i] = isIn;
        }

        // --- Current Progress Bar (dirty-rect: only refill when width changes or idle) ---
        {
            uint32_t abs_mA = (i_mA >= 0) ? static_cast<uint32_t>(i_mA) : static_cast<uint32_t>(-i_mA);
            uint8_t w = (abs_mA * Display::BAR_W) / Display::A_MAX_mA;
            if (w > Display::BAR_W) w = Display::BAR_W;

            const bool idle = (v_mV < 1000);

            if (idle || w != prevABarW[i]) {
                lcd.fillRectangleFast(colX, Display::A_BAR_Y,
                                      Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);

                if (idle) {
                    lcd.fillRectangleFast(colX + animPos, Display::A_BAR_Y,
                                          Display::SEG_WIDTH, Display::BAR_H,
                                          currentBarColor(i_mA, isIn));
                } else if (w > 0) {
                    lcd.fillRectangleFast(colX, Display::A_BAR_Y,
                                          w, Display::BAR_H,
                                          currentBarColor(i_mA, isIn));
                }
                prevABarW[i] = w;
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

    // Per-channel IN+/IN- direction correction:
    // CH1, CH2: normal (IN+ = bus side, IN- = load side)
    // CH3:      reversed (pins swapped on PCB, negate shunt voltage)
    ina3221.setChannelDirectionReversed(1, false);  // normal
    ina3221.setChannelDirectionReversed(2, false);  // normal
    ina3221.setChannelDirectionReversed(3, true);   // reversed

    // Initialize Button (KEY1 on PA1, active low)
    BUTTON_Config btnConfig = BUTTON_Config::getDefault();
    Button keyButton(KEY1_GPIO_Port, KEY1_Pin, btnConfig);

    // Initialize display layout
    DisplayInit();

    // Show INA3221 found status on status bar briefly, then clear
    lcd.writeString(0, 0, "INA3221 Ready", Font_7x10,
                    ST7735_Color::GREEN, ST7735_Color::BLACK);
    HAL_Delay(800);
    lcd.fillRectangleFast(0, 0, 160, 10, ST7735_Color::BLACK);

    // Start TIM3 for 200ms periodic tick
    HAL_TIM_Base_Start_IT(&htim3);

    // Measurement data
    INA3221_ChannelData channelData[3];
    char mainBuf[32];

while (1)
{
    // Wait for TIM3 tick (non-blocking)
    if (tim3_tick_flag)
    {
        tim3_tick_flag = 0;

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

            // Restore display & reinitialize charge
            DisplayInit();
            HAL_TIM_Base_Start_IT(&htim3);  // re-start if stopped
        }
    }
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