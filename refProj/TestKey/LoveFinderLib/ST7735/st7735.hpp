/**
 * @file st7735.hpp
 * @brief ST7735 LCD Driver - C++17
 */
#ifndef ST7735_HPP
#define ST7735_HPP

#include "main.h"
#include "fonts.h"
#include "icons.h"
#include <cstdint>
#include <cstddef>

/*============================================================================
 * C++17 常量定义 (constexpr 替代 #define)
 *============================================================================*/

namespace ST7735 {

// Memory Access Control
constexpr uint8_t MADCTL_MY  = 0x80;
constexpr uint8_t MADCTL_MX  = 0x40;
constexpr uint8_t MADCTL_MV  = 0x20;
constexpr uint8_t MADCTL_ML  = 0x10;
constexpr uint8_t MADCTL_RGB = 0x00;
constexpr uint8_t MADCTL_BGR = 0x08;
constexpr uint8_t MADCTL_MH  = 0x04;

// Hardware Configuration
constexpr uint8_t  XSTART  = 0;
constexpr uint8_t  YSTART  = 24;
constexpr uint16_t WIDTH   = 160;
constexpr uint16_t HEIGHT  = 80;
constexpr uint8_t  ROTATION = (MADCTL_MY | MADCTL_MV | MADCTL_BGR);

// Commands
constexpr uint8_t NOP     = 0x00;
constexpr uint8_t SWRESET = 0x01;
constexpr uint8_t RDDID   = 0x04;
constexpr uint8_t RDDST   = 0x09;
constexpr uint8_t SLPIN   = 0x10;
constexpr uint8_t SLPOUT  = 0x11;
constexpr uint8_t PTLON   = 0x12;
constexpr uint8_t NORON   = 0x13;
constexpr uint8_t INVOFF  = 0x20;
constexpr uint8_t INVON   = 0x21;
constexpr uint8_t GAMSET  = 0x26;
constexpr uint8_t DISPOFF = 0x28;
constexpr uint8_t DISPON  = 0x29;
constexpr uint8_t CASET   = 0x2A;
constexpr uint8_t RASET   = 0x2B;
constexpr uint8_t RAMWR   = 0x2C;
constexpr uint8_t RAMRD   = 0x2E;
constexpr uint8_t PTLAR   = 0x30;
constexpr uint8_t COLMOD  = 0x3A;
constexpr uint8_t MADCTL  = 0x36;
constexpr uint8_t FRMCTR1 = 0xB1;
constexpr uint8_t FRMCTR2 = 0xB2;
constexpr uint8_t FRMCTR3 = 0xB3;
constexpr uint8_t INVCTR  = 0xB4;
constexpr uint8_t DISSET5 = 0xB6;
constexpr uint8_t PWCTR1  = 0xC0;
constexpr uint8_t PWCTR2  = 0xC1;
constexpr uint8_t PWCTR3  = 0xC2;
constexpr uint8_t PWCTR4  = 0xC3;
constexpr uint8_t PWCTR5  = 0xC4;
constexpr uint8_t VMCTR1  = 0xC5;
constexpr uint8_t RDID1   = 0xDA;
constexpr uint8_t RDID2   = 0xDB;
constexpr uint8_t RDID3   = 0xDC;
constexpr uint8_t RDID4   = 0xDD;
constexpr uint8_t PWCTR6  = 0xFC;
constexpr uint8_t GMCTRP1 = 0xE0;
constexpr uint8_t GMCTRN1 = 0xE1;

// Colors (RGB565)
constexpr uint16_t BLACK   = 0x0000;
constexpr uint16_t BLUE    = 0x001F;
constexpr uint16_t RED     = 0xF800;
constexpr uint16_t GREEN   = 0x07E0;
constexpr uint16_t CYAN    = 0x07FF;
constexpr uint16_t MAGENTA = 0xF81F;
constexpr uint16_t YELLOW  = 0xFFE0;
constexpr uint16_t WHITE   = 0xFFFF;

// Color conversion macro
constexpr uint16_t Color565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

// Custom colors
constexpr uint16_t ORANGE  = Color565(255, 165, 0);

} // namespace ST7735

// Backward compatible macros
#define ST7735_MADCTL_MY    ST7735::MADCTL_MY
#define ST7735_MADCTL_MX    ST7735::MADCTL_MX
#define ST7735_MADCTL_MV    ST7735::MADCTL_MV
#define ST7735_MADCTL_ML    ST7735::MADCTL_ML
#define ST7735_MADCTL_RGB   ST7735::MADCTL_RGB
#define ST7735_MADCTL_BGR   ST7735::MADCTL_BGR
#define ST7735_MADCTL_MH    ST7735::MADCTL_MH
#define ST7735_XSTART       ST7735::XSTART
#define ST7735_YSTART       ST7735::YSTART
#define ST7735_WIDTH        ST7735::WIDTH
#define ST7735_HEIGHT       ST7735::HEIGHT
#define ST7735_ROTATION     ST7735::ROTATION
#define ST7735_BLACK        ST7735::BLACK
#define ST7735_BLUE         ST7735::BLUE
#define ST7735_RED          ST7735::RED
#define ST7735_GREEN        ST7735::GREEN
#define ST7735_CYAN         ST7735::CYAN
#define ST7735_MAGENTA      ST7735::MAGENTA
#define ST7735_YELLOW       ST7735::YELLOW
#define ST7735_WHITE        ST7735::WHITE
#define ST7735_ORANGE       ST7735::ORANGE
#define ST7735_COLOR565(r,g,b) ST7735::Color565(r,g,b)

// Command aliases
#define ST7735_NOP          ST7735::NOP
#define ST7735_SWRESET      ST7735::SWRESET
#define ST7735_RDDID        ST7735::RDDID
#define ST7735_RDDST        ST7735::RDDST
#define ST7735_SLPIN        ST7735::SLPIN
#define ST7735_SLPOUT       ST7735::SLPOUT
#define ST7735_PTLON        ST7735::PTLON
#define ST7735_NORON        ST7735::NORON
#define ST7735_CASET        ST7735::CASET
#define ST7735_RASET        ST7735::RASET
#define ST7735_RAMWR        ST7735::RAMWR
#define ST7735_RAMRD        ST7735::RAMRD
#define ST7735_PTLAR        ST7735::PTLAR
#define ST7735_INVOFF       ST7735::INVOFF
#define ST7735_INVON        ST7735::INVON
#define ST7735_DISPON       ST7735::DISPON
#define ST7735_DISPOFF      ST7735::DISPOFF
#define ST7735_MADCTL       ST7735::MADCTL
#define ST7735_COLMOD       ST7735::COLMOD
#define ST7735_GMCTRP1      ST7735::GMCTRP1
#define ST7735_GMCTRN1      ST7735::GMCTRN1
#define ST7735_GAMSET       ST7735::GAMSET
#define ST7735_FRMCTR1      ST7735::FRMCTR1
#define ST7735_FRMCTR2      ST7735::FRMCTR2
#define ST7735_FRMCTR3      ST7735::FRMCTR3
#define ST7735_INVCTR       ST7735::INVCTR
#define ST7735_DISSET5      ST7735::DISSET5
#define ST7735_PWCTR1       ST7735::PWCTR1
#define ST7735_PWCTR2       ST7735::PWCTR2
#define ST7735_PWCTR3       ST7735::PWCTR3
#define ST7735_PWCTR4       ST7735::PWCTR4
#define ST7735_PWCTR5       ST7735::PWCTR5
#define ST7735_PWCTR6       ST7735::PWCTR6
#define ST7735_VMCTR1       ST7735::VMCTR1
#define ST7735_RDID1        ST7735::RDID1
#define ST7735_RDID2        ST7735::RDID2
#define ST7735_RDID3        ST7735::RDID3
#define ST7735_RDID4        ST7735::RDID4

// SPI Port
#define ST7735_SPI_PORT     hspi1
extern SPI_HandleTypeDef ST7735_SPI_PORT;

// Pin definitions
#define ST7735_RES_Pin      LCD_RESET_Pin
#define ST7735_RES_GPIO_Port LCD_RESET_GPIO_Port
#define ST7735_CS_Pin       LCD_CS_Pin
#define ST7735_CS_GPIO_Port LCD_CS_GPIO_Port
#define ST7735_DC_Pin       LCD_DC_Pin
#define ST7735_DC_GPIO_Port LCD_DC_GPIO_Port

/*============================================================================
 * Gamma enum class
 *============================================================================*/
enum class GammaDef : uint8_t {
    GAMMA_10 = 0x01,
    GAMMA_25 = 0x02,
    GAMMA_22 = 0x04,
    GAMMA_18 = 0x08
};

/*============================================================================
 * C++ API Functions
 *============================================================================*/
extern "C" {

void ST7735_Unselect();
void ST7735_Init();
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7735_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_FillRectangle_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_FillScreen(uint16_t color);
void ST7735_FillScreenFast(uint16_t color);
void ST7735_FillScreen_DMA(uint16_t color);
void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
void ST7735_DrawImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
void ST7735_InvertColors(bool invert);
void ST7735_SetGamma(GammaDef gamma);
void ST7735_Print(uint16_t x, uint16_t y, FontDef font, uint16_t color, uint16_t bgcolor, const char* format, ...);

// Icon functions
void ST7735_DrawIcon(uint16_t x, uint16_t y, IconIndex icon);

// Graphics primitives
void ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void ST7735_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color);
void ST7735_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color);

// Utility functions
void ST7735_DrawValueWithIcon(uint16_t x, uint16_t y, IconIndex icon,
                              const char* value, const char* unit,
                              uint16_t valueColor, uint16_t unitColor);

} // extern "C"

#endif // ST7735_HPP
