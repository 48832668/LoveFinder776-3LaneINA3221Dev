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

// LED blink control (declared in tim.c) - disabled, no heartbeat needed
extern volatile uint8_t led1_blink_enabled;

/*============================================================================
 * Constants
 *============================================================================*/

// INA3221 address: A0=VS => 0x41
constexpr uint8_t INA3221_ADDR = 0x41;
// Shunt resistor: 5mΩ per channel (hardware updated)
constexpr ShuntConfig INA3221_SHUNT = ShuntConfig::all(0.005f);

// Display layout (160x80 LCD)
namespace Display {
    // Column positions for 3-channel display
    constexpr uint8_t COL1_X = 0;    // CH1
    constexpr uint8_t COL2_X = 48;   // CH2
    constexpr uint8_t COL3_X = 96;   // CH3
    constexpr uint8_t COL_W  = 47;   // column width (leaves 1px gap before badge)

    // Row Y positions (all text uses Font_Style2_9x18, 18px height)
    constexpr uint8_t STATUS_Y    = 0;     // Font_Style2_9x18: CHx (left) + input voltage (right)
    constexpr uint8_t POWER_Y     = 19;    // Font_Style2_9x18: total power (left) + mAh (right)
    constexpr uint8_t CURRENT_Y   = 39;    // Font_Style2_9x18: current XX.YY per column
    constexpr uint8_t POWER_COL_Y = 60;    // Font_Style2_9x18: per-channel power X.X

    // Unit badge on the right of each row
    constexpr uint8_t BADGE_X   = 143;   // right side of 160px screen
    constexpr uint8_t BADGE_W   = 15;
    constexpr uint8_t BADGE_R   = 3;

    // Rounded rect around power row
    constexpr uint8_t POWER_RECT_X = 1;
    constexpr uint8_t POWER_RECT_Y = 17;    // shifted down for taller Font_Style2_9x18 status row
    constexpr uint8_t POWER_RECT_W = 158;
    constexpr uint8_t POWER_RECT_H = 22;    // 2px padding top/bottom for 18px font
    constexpr uint8_t POWER_RADIUS = 4;

    // Progress bar positions (directly below each value text, no gap)
    constexpr uint8_t V_BAR_Y   = CURRENT_Y + 18;  // 57 — below current text
    constexpr uint8_t A_BAR_Y   = POWER_COL_Y + 18;  // 78 — below per-channel power text

    // Progress bar dimensions
    constexpr uint8_t BAR_W     = 45;    // bar width (within 47px col)
    constexpr uint8_t BAR_H     = 2;     // bar height (reduced to fit 80px screen)

    // Max values for percentage
    constexpr uint16_t V_MAX_mV = 26000;  // 26V
    constexpr int32_t  A_MAX_mA = 10000;  // 10A (user specified)
    constexpr uint16_t P_MAX_x10 = 1000;  // 100.0W max for per-channel power bar

    // Progress bar animation
    constexpr uint8_t SEG_WIDTH  = 12;    // animated slider width (matching reference)
    constexpr uint8_t MAX_POS    = BAR_W - SEG_WIDTH;  // 33
    constexpr uint8_t ANIM_FRAMES = MAX_POS * 2;       // 66

    // (OLD badge constants LABEL_BG_W/H/Y, LABEL_X_PAD removed - no longer used)
}

// Column X positions array for indexed access
constexpr uint8_t COL_X[3] = {
    Display::COL1_X,
    Display::COL2_X,
    Display::COL3_X
};

// Per-channel accent colors (for power text, power bar, CHx label)
constexpr uint16_t CH_COLOR[3] = {
    ST7735_Color::RGB565(0,   200, 255),   // CH1: bright cyan
    ST7735_Color::RGB565(255, 200, 0),     // CH2: amber/gold
    ST7735_Color::RGB565(255, 100, 200),   // CH3: pink/magenta
};

// Dimmed variant for current text/bar (lower saturation, same hue)
constexpr uint16_t CH_COLOR_DIM[3] = {
    ST7735_Color::RGB565(0,   140, 190),   // CH1: dimmer cyan
    ST7735_Color::RGB565(190, 150, 0),     // CH2: dimmer amber
    ST7735_Color::RGB565(190, 70,  150),   // CH3: dimmer pink
};

/*============================================================================
 * Global Variables
 *============================================================================*/

I2C i2cDriver(&hi2c1);
EEPROM eeprom;
INA3221 ina3221;
ST7735 lcd;

// Previous values for change detection
static int32_t  prevCurrent_mA[3] = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
static bool     prevIsIn[3] = {false, false, false};  // for IN centering switch
static uint16_t prevPower_x10[3] = {0xFFFF, 0xFFFF, 0xFFFF};  // per-channel power *10
static uint8_t  prevVBarW[3] = {0xFF, 0xFF, 0xFF};    // previous current bar widths
static uint8_t  prevABarW[3] = {0xFF, 0xFF, 0xFF};    // previous power bar widths

// Animation state for idle bars
static uint8_t animCounter = 0;

// Power / charge display tracking
static uint16_t prevPowerW_x100 = 0xFFFF;       // for dirty detect: W*100
static uint16_t prevInputVoltage_mV = 0xFFFF;  // for dirty detect
static uint16_t prevCharge_mAh = 0xFFFF;       // for dirty detect
static int8_t   prevInputChannel = -2;         // -2 = uninitialized

// Charge accumulation (200ms tick)
static uint32_t charge_mAs = 0;                // accumulated charge in mAs
static uint32_t chargeFraction = 0;            // fractional accumulator

/*============================================================================
 * Function Prototypes
 *============================================================================*/

void SystemClock_Config(void);
void InitPeripherals(void);
void DisplayInit(void);
void UpdateDisplay(const INA3221_ChannelData data[3]);

/*============================================================================
 * Display Functions
 *============================================================================*/

// Fill a rounded rectangle using fillRectangleFast for the body + corners
// Implemented as 4 corner squares + 3 fill rectangles (top, middle, bottom)
static void fillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                          uint8_t r, uint16_t color)
{
    if (r == 0 || r > h / 2 || r > w / 2) {
        lcd.fillRectangleFast(x, y, w, h, color);
        return;
    }

    // Body (full width, excluding corner heights)
    lcd.fillRectangleFast(x, y + r, w, h - 2 * r, color);

    // Top and bottom bars (full width minus corner width)
    lcd.fillRectangleFast(x + r, y, w - 2 * r, r, color);
    lcd.fillRectangleFast(x + r, y + h - r, w - 2 * r, r, color);

    // Corner pixels (4 squares at each corner)
    for (uint8_t dy = 0; dy < r; dy++) {
        // Determine how many pixels from the edge are inside the circle
        // distance^2 = (r-1-dx)^2 + (r-1-dy)^2 <= r^2
        uint8_t dxLen = 0;
        for (uint8_t dx = 0; dx < r; dx++) {
            int16_t dx_off = static_cast<int16_t>(r - 1 - dx);
            int16_t dy_off = static_cast<int16_t>(r - 1 - dy);
            if (dx_off * dx_off + dy_off * dy_off <= static_cast<int16_t>(r * r)) {
                dxLen = dx + 1;
            }
        }
        if (dxLen > 0) {
            // Top-left corner
            lcd.fillRectangleFast(x, y + dy, dxLen, 1, color);
            // Top-right corner
            lcd.fillRectangleFast(x + w - dxLen, y + dy, dxLen, 1, color);
            // Bottom-left corner
            lcd.fillRectangleFast(x, y + h - 1 - dy, dxLen, 1, color);
            // Bottom-right corner
            lcd.fillRectangleFast(x + w - dxLen, y + h - 1 - dy, dxLen, 1, color);
        }
    }
}

void DisplayInit(void)
{
    lcd.fillScreen(ST7735_Color::BLACK);

    // Clear status bar area (top 18px for Font_Style2_9x18)
    lcd.fillRectangleFast(0, Display::STATUS_Y, 160, 18, ST7735_Color::BLACK);

    // Draw power rounded rectangle background (static, done once)
    fillRoundRect(Display::POWER_RECT_X, Display::POWER_RECT_Y,
                  Display::POWER_RECT_W, Display::POWER_RECT_H,
                  Display::POWER_RADIUS, ST7735_Color::DARK_GRAY);

    // Clear current row area
    lcd.fillRectangleFast(0, Display::CURRENT_Y, 160, 18, ST7735_Color::BLACK);

    // Clear per-channel power row area
    lcd.fillRectangleFast(0, Display::POWER_COL_Y, 160, 18, ST7735_Color::BLACK);

    // Draw unit badge backgrounds (static, done once)
    // Note: badges use Font_Style2_9x18, matching current/power text font
    fillRoundRect(Display::BADGE_X, Display::CURRENT_Y,
                  Display::BADGE_W, 18,
                  Display::BADGE_R, ST7735_Color::DARK_GRAY);
    fillRoundRect(Display::BADGE_X, Display::POWER_COL_Y,
                  Display::BADGE_W, 18,
                  Display::BADGE_R, ST7735_Color::DARK_GRAY);

    // Write unit labels inside badges (Font_Style2_9x18, matching value font)
    lcd.writeString(Display::BADGE_X + 3, Display::CURRENT_Y,
                    "A", Font_Style2_9x18,
                    ST7735_Color::RGB565(100, 200, 255), ST7735_Color::DARK_GRAY);
    lcd.writeString(Display::BADGE_X + 3, Display::POWER_COL_Y,
                    "W", Font_Style2_9x18,
                    ST7735_Color::RGB565(230, 140, 30), ST7735_Color::DARK_GRAY);

    // Draw progress bar backgrounds (static, done once)
    for (int i = 0; i < 3; i++) {
        lcd.fillRectangleFast(COL_X[i], Display::V_BAR_Y,
                              Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
        lcd.fillRectangleFast(COL_X[i], Display::A_BAR_Y,
                              Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
    }

    // Force first update to draw all values
    prevCurrent_mA[0] = prevCurrent_mA[1] = prevCurrent_mA[2] = 0x7FFFFFFF;
    prevIsIn[0] = prevIsIn[1] = prevIsIn[2] = false;
    prevPower_x10[0] = prevPower_x10[1] = prevPower_x10[2] = 0xFFFF;
    prevVBarW[0] = prevVBarW[1] = prevVBarW[2] = 0xFF;
    prevABarW[0] = prevABarW[1] = prevABarW[2] = 0xFF;
    prevPowerW_x100 = 0xFFFF;
    prevInputVoltage_mV = 0xFFFF;
    prevCharge_mAh = 0xFFFF;
    prevInputChannel = -2;  // force first draw

    // Reset charge accumulation
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

    // --- Identify input channel & calculate total power ---
    int inputChannel = -1;   // -1 = no input detected
    uint16_t inputVoltage_mV = 0;
    uint32_t totalW_x10 = 0;
    uint32_t totalW_x100 = 0;

    for (int ch = 0; ch < 3; ch++) {
        const INA3221_ChannelData& chData = data[ch];

        // CH3 forced OUT (3.3V derived from its DC connector, direction unreliable)
        INA3221_Direction dir;
        if (ch == 2) {
            dir = INA3221_Direction::OUT;
        } else {
            dir = INA3221::getDirection(chData.shuntVoltage_uV);
        }

        if (dir == INA3221_Direction::IN) {
            inputChannel = ch;  // 0=CH1, 1=CH2
            inputVoltage_mV = chData.busVoltage_mV;

            // power = V * |I| / 1000 (mW)
            uint32_t abs_mA = (chData.current_mA >= 0)
                                ? static_cast<uint32_t>(chData.current_mA)
                                : static_cast<uint32_t>(-chData.current_mA);
            totalW_x10 += (static_cast<uint32_t>(chData.busVoltage_mV) * abs_mA) / 100000;
            totalW_x100 += (static_cast<uint32_t>(chData.busVoltage_mV) * abs_mA) / 10000;
        }
    }

    // If no channel detected as IN (all OUT), check CH3 bus voltage
    // CH3's direction is unreliable but its bus voltage is valid
    if (inputChannel < 0) {
        const INA3221_ChannelData& chData = data[2];  // CH3
        if (chData.busVoltage_mV >= 3000) {            // ≥3V = valid power source
            inputChannel = 2;                          // CH3
            inputVoltage_mV = chData.busVoltage_mV;

            // Calculate CH3 power contribution
            uint32_t abs_mA = (chData.current_mA >= 0)
                                ? static_cast<uint32_t>(chData.current_mA)
                                : static_cast<uint32_t>(-chData.current_mA);
            totalW_x10 += (static_cast<uint32_t>(chData.busVoltage_mV) * abs_mA) / 100000;
            totalW_x100 += (static_cast<uint32_t>(chData.busVoltage_mV) * abs_mA) / 10000;
        }
    }

    // Clamp totalW_x10 / x100
    if (totalW_x10 > 9999) totalW_x10 = 9999;
    if (totalW_x100 > 999999) totalW_x100 = 999999;

    // --- Charge accumulation (200ms tick from IN channel only) ---
    if (inputChannel >= 0) {
        const INA3221_ChannelData& inCh = data[inputChannel];
        uint32_t abs_mA = (inCh.current_mA >= 0)
                            ? static_cast<uint32_t>(inCh.current_mA)
                            : static_cast<uint32_t>(-inCh.current_mA);
        // Each 200ms tick: accumulate abs_mA * 0.2 mAs (fractional = /5)
        chargeFraction += abs_mA;
        while (chargeFraction >= 5) {
            charge_mAs += 1;
            chargeFraction -= 5;
        }
    }

    // --- Row 1: Status Bar (Y=0, Font_Style2_9x18) ---
    // Left: CH1/CH2/CH3 (input source) in the channel's own color
    if (inputChannel != prevInputChannel) {
        // Map inputChannel (0=CH1, 1=CH2, 2=CH3) to display column index (2-CH3, 1-CH2, 0-CH1)
        // so CHx label color matches the column's power text color (CH_COLOR[col])
        int colIdx = (inputChannel >= 0) ? (2 - inputChannel) : -1;
        uint16_t chColor = (colIdx >= 0) ? CH_COLOR[colIdx] : ST7735_Color::GRAY;
        if (inputChannel >= 0) {
            snprintf(buf, sizeof(buf), "CH%d", inputChannel + 1);
        } else {
            snprintf(buf, sizeof(buf), "---");
        }
        // Clear old 18px text area to prevent ghosting
        lcd.fillRectangleFast(0, Display::STATUS_Y, 160, 18, ST7735_Color::BLACK);
        lcd.writeString(0, Display::STATUS_Y, buf, Font_Style2_9x18,
                        chColor, ST7735_Color::BLACK);
        prevInputChannel = static_cast<int8_t>(inputChannel);
    }

    // Right: input voltage XX.YYV (right-aligned with Font_Style2_9x18: 7 chars × 9px = 63px)
    {
        if (inputVoltage_mV != prevInputVoltage_mV) {
            uint16_t v_int = inputVoltage_mV / 1000;
            uint8_t  v_dec = (inputVoltage_mV % 1000) / 10;
            snprintf(buf, sizeof(buf), "%02u.%02uV", v_int, v_dec);
            lcd.writeString(106, Display::STATUS_Y, buf, Font_Style2_9x18,
                            ST7735_Color::RGB565(180, 220, 100), ST7735_Color::BLACK);
            prevInputVoltage_mV = inputVoltage_mV;
        }
    }

    // --- Row 2: Power + Charge (Font_Style2_9x18 inside rounded rect) ---
    // Always redraw both together (they're close enough to overlap)
    {
        uint16_t w_int  = static_cast<uint16_t>(totalW_x100 / 100);
        uint8_t  w_dec  = static_cast<uint8_t>(totalW_x100 % 100);
        uint16_t mAh = static_cast<uint16_t>(charge_mAs / 3600);

        if (totalW_x100 != prevPowerW_x100 || mAh != prevCharge_mAh) {
            snprintf(buf, sizeof(buf), "%04u.%02uW", w_int, w_dec);
            lcd.writeString(Display::POWER_RECT_X + 2, Display::POWER_Y,
                            buf, Font_Style2_9x18,
                            ST7735_Color::WHITE, ST7735_Color::DARK_GRAY);

            // Right-align mAh: 8 chars × 9px = 72px
            snprintf(buf, sizeof(buf), "%05umAh", mAh);
            lcd.writeString(Display::POWER_RECT_X + Display::POWER_RECT_W - 72 - 2,
                            Display::POWER_Y, buf, Font_Style2_9x18,
                            ST7735_Color::RGB565(100, 200, 255), ST7735_Color::DARK_GRAY);

            prevPowerW_x100 = static_cast<uint16_t>(totalW_x100);
            prevCharge_mAh = mAh;
        }
    }

    // --- Three columns: Current (Y=35) + Per-Channel Power (Y=56) ---
    for (int i = 0; i < 3; i++)
    {
        const uint8_t colX = COL_X[i];
        const INA3221_ChannelData& ch = data[2 - i];  // display order: CH3, CH2, CH1
        const uint16_t v_mV = ch.busVoltage_mV;
        const int32_t  i_mA = ch.current_mA;

        // CH3 forced OUT, CH1/CH2 use normal detection
        const int chIdx = 2 - i;  // actual channel index (0=CH1, 1=CH2, 2=CH3)
        INA3221_Direction dir;
        if (chIdx == 2) {
            dir = INA3221_Direction::OUT;
        } else {
            dir = INA3221::getDirection(ch.shuntVoltage_uV);
        }
        const bool isIn = (dir == INA3221_Direction::IN);

        // Pre-calc power for this channel: V * |I| / 100000 → W*10
        uint32_t abs_mA = (i_mA >= 0) ? static_cast<uint32_t>(i_mA) : static_cast<uint32_t>(-i_mA);
        uint16_t power_x10 = static_cast<uint16_t>((static_cast<uint32_t>(v_mV) * abs_mA) / 100000);
        if (power_x10 > 999) power_x10 = 999;  // clamp to 99.9W

        // --- Current (Font_Style2_9x18, left-aligned with A unit) ---
        if (i_mA != prevCurrent_mA[i] || isIn != prevIsIn[i])
        {
            uint32_t abs_mA2 = abs_mA;
            if (abs_mA2 > 99990) abs_mA2 = 99990;

            uint8_t int_part = static_cast<uint8_t>(abs_mA2 / 1000);
            uint8_t dec_part = static_cast<uint8_t>((abs_mA2 % 1000) / 10);
            snprintf(buf, sizeof(buf), "%02u.%02u", int_part, dec_part);

            uint16_t cColor = CH_COLOR_DIM[i];

            lcd.writeString(colX, Display::CURRENT_Y,
                            buf, Font_Style2_9x18,
                            cColor, ST7735_Color::BLACK);

            prevCurrent_mA[i] = i_mA;
            prevIsIn[i] = isIn;
        }

        // --- Current Progress Bar ---
        {
            uint8_t w = (abs_mA * Display::BAR_W) / Display::A_MAX_mA;
            if (w > Display::BAR_W) w = Display::BAR_W;

            const bool idle = (v_mV < 1000);

            if (idle || w != prevVBarW[i]) {
                lcd.fillRectangleFast(colX, Display::V_BAR_Y,
                                      Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
                if (idle) {
                    lcd.fillRectangleFast(colX + animPos, Display::V_BAR_Y,
                                          Display::SEG_WIDTH, Display::BAR_H,
                                          CH_COLOR_DIM[i]);
                } else if (w > 0) {
                    lcd.fillRectangleFast(colX, Display::V_BAR_Y,
                                          w, Display::BAR_H,
                                          CH_COLOR_DIM[i]);
                }
                prevVBarW[i] = w;
            }
        }

        // --- Per-Channel Power (Font_Style2_9x18, %03u.%u, color per channel) ---
        if (power_x10 != prevPower_x10[i])
        {
            uint8_t p_int = static_cast<uint8_t>(power_x10 / 10);
            uint8_t p_dec = static_cast<uint8_t>(power_x10 % 10);
            snprintf(buf, sizeof(buf), "%03u.%u", p_int, p_dec);

            // Clear entire column area to prevent ghosting when width changes
            lcd.fillRectangleFast(colX, Display::POWER_COL_Y,
                                  Display::COL_W, 18, ST7735_Color::BLACK);

            lcd.writeString(colX, Display::POWER_COL_Y,
                            buf, Font_Style2_9x18,
                            CH_COLOR[i], ST7735_Color::BLACK);

            prevPower_x10[i] = power_x10;
        }

        // --- Power Progress Bar ---
        {
            uint8_t w = (static_cast<uint32_t>(power_x10) * Display::BAR_W) / Display::P_MAX_x10;
            if (w > Display::BAR_W) w = Display::BAR_W;

            const bool idle = (v_mV < 1000);

            if (idle || w != prevABarW[i]) {
                lcd.fillRectangleFast(colX, Display::A_BAR_Y,
                                      Display::BAR_W, Display::BAR_H, ST7735_Color::GUNMETAL);
                if (idle) {
                    lcd.fillRectangleFast(colX + animPos, Display::A_BAR_Y,
                                          Display::SEG_WIDTH, Display::BAR_H,
                                          CH_COLOR[i]);
                } else if (w > 0) {
                    lcd.fillRectangleFast(colX, Display::A_BAR_Y,
                                          w, Display::BAR_H,
                                          CH_COLOR[i]);
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

    // Disable LED heartbeat blink (TIM3 ISR toggles LED1 every 200ms)
    led1_blink_enabled = 0;

    // Initialize ST7735 LCD (new C++ class API)
    lcd.init(&hspi1,
             LCD_CS_GPIO_Port, LCD_CS_Pin,
             LCD_DC_GPIO_Port, LCD_DC_Pin,
             LCD_RESET_GPIO_Port, LCD_RESET_Pin,
             LCD_EN_GPIO_Port, LCD_EN_Pin,
             "LCD_MAIN");
    lcd.begin();
    lcd.fillScreen(ST7735_Color::BLACK);

    // ── Splash screen: show "你好" ─────────────────────────────────
    lcd.writeStringChineseDMA(48, 24, "\xE4\xBD\xA0\xE5\xA5\xBD", Font_Style3_ZH_16x16,
                              ST7735_Color::GREEN, ST7735_Color::BLACK);
    HAL_Delay(1500);
    lcd.fillScreen(ST7735_Color::BLACK);

    // Initialize EEPROM (for I2C bus presence, not actively used)
    eeprom.init(&hi2c1);

    // Initialize INA3221
    if (!ina3221.init(&hi2c1, INA3221_ADDR, INA3221_SHUNT))
    {
        // INA3221 not found - show error
        lcd.writeString(30, 30, "INA3221 Not Found!", Font_Style1_7x10,
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
            lcd.writeString(30, 0, "I2C Devices", Font_Style1_7x10,
                            ST7735_Color::CYAN, ST7735_Color::BLACK);

            uint8_t devices[16];
            uint8_t count = i2cDriver.scan(devices, sizeof(devices));
            uint8_t y = 14;
            for (uint8_t j = 0; j < count; j++)
            {
                snprintf(mainBuf, sizeof(mainBuf), "0x%02X", devices[j]);
                lcd.writeString(5, y, mainBuf, Font_Style1_7x10,
                                ST7735_Color::WHITE, ST7735_Color::BLACK);
                y += 12;
            }
            HAL_Delay(2000);

            // Restore display (re-draws everything fresh from DisplayInit)
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