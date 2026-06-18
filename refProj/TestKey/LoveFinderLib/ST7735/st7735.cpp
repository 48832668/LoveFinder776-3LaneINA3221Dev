/**
 * @file st7735.cpp
 * @brief ST7735 LCD Driver Implementation - C++17
 */
#include "main.h"
#include "st7735.hpp"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint8_t DELAY = 0x80;
}

// 前向声明
static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
static void ST7735_WriteData_DMA(uint8_t *buff, size_t buff_size);

// based on Adafruit ST7735 library for Arduino
static const uint8_t
    init_cmds1[] = {           // Init for 7735R, part 1 (red or green tab)
        15,                    // 15 commands in list:
        ST7735_SWRESET, DELAY, //  1: Software reset, 0 args, w/delay
        150,                   //     150 ms delay
        ST7735_SLPOUT, DELAY,  //  2: Out of sleep mode, 0 args, w/delay
        255,                   //     500 ms delay
        ST7735_FRMCTR1, 3,     //  3: Frame rate ctrl - normal mode, 3 args:
        0x01, 0x2C, 0x2D,      //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
        ST7735_FRMCTR2, 3,     //  4: Frame rate control - idle mode, 3 args:
        0x01, 0x2C, 0x2D,      //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
        ST7735_FRMCTR3, 6,     //  5: Frame rate ctrl - partial mode, 6 args:
        0x01, 0x2C, 0x2D,      //     Dot inversion mode
        0x01, 0x2C, 0x2D,      //     Line inversion mode
        ST7735_INVCTR, 1,      //  6: Display inversion ctrl, 1 arg, no delay:
        0x07,                  //     No inversion
        ST7735_PWCTR1, 3,      //  7: Power control, 3 args, no delay:
        0xA2,
        0x02,             //     -4.6V
        0x84,             //     AUTO mode
        ST7735_PWCTR2, 1, //  8: Power control, 1 arg, no delay:
        0xC5,             //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
        ST7735_PWCTR3, 2, //  9: Power control, 2 args, no delay:
        0x0A,             //     Opamp current small
        0x00,             //     Boost frequency
        ST7735_PWCTR4, 2, // 10: Power control, 2 args, no delay:
        0x8A,             //     BCLK/2, Opamp current small & Medium low
        0x2A,
        ST7735_PWCTR5, 2, // 11: Power control, 2 args, no delay:
        0x8A, 0xEE,
        ST7735_VMCTR1, 1, // 12: Power control, 1 arg, no delay:
        0x0E,
        ST7735_INVOFF, 0, // 13: Don't invert display, no args, no delay
        ST7735_MADCTL, 1, // 14: Memory access control (directions), 1 arg:
        ST7735_ROTATION,  //     row addr/col addr, bottom to top refresh
        ST7735_COLMOD, 1, // 15: set color mode, 1 arg, no delay:
        0x05},            //     16-bit color

    init_cmds2[] = {      // Init for 7735S, part 2 (160x80 display)
        3,                //  3 commands in list:
        ST7735_CASET, 4,  //  1: Column addr set, 4 args, no delay:
        0x00, 0x00,       //     XSTART = 0
        0x00, 0x4F,       //     XEND = 79
        ST7735_RASET, 4,  //  2: Row addr set, 4 args, no delay:
        0x00, 0x00,       //     XSTART = 0
        0x00, 0x9F,       //     XEND = 159
        ST7735_INVOFF, 1}, //  3: Invert colors 此处我修改为INVOFF

    init_cmds3[] = {                                                                                                         // Init for 7735R, part 3 (red or green tab)
        4,                                                                                                                   //  4 commands in list:
        ST7735_GMCTRP1, 16,                                                                                                  //  1: Gamma Adjustments (pos. polarity), 16 args, no delay:
        0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10, ST7735_GMCTRN1, 16,  //  2: Gamma Adjustments (neg. polarity), 16 args, no delay:
        0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10, ST7735_NORON, DELAY, //  3: Normal display on, no args, w/delay
        10,                                                                                                                  //     10 ms delay
        ST7735_DISPON, DELAY,                                                                                                //  4: Main screen turn on, no args w/delay
        100};                                                                                                                //     100 ms delay

static void ST7735_Select()
{
  HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_RESET);
}

void ST7735_Unselect()
{
  HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_SET);
}

static void ST7735_Reset()
{
  HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_RESET);
  HAL_Delay(5);
  HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_SET);
}

static void ST7735_WriteCommand(uint8_t cmd)
{
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&ST7735_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ST7735_WriteData(uint8_t *buff, size_t buff_size)
{
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  HAL_SPI_Transmit(&ST7735_SPI_PORT, buff, buff_size, HAL_MAX_DELAY);
}

/*============================================================================
 * DMA传输函数 - 高性能刷屏
 *============================================================================*/

// DMA传输完成标志
static volatile uint8_t dma_transfer_complete = 1;

// DMA传输数据（阻塞式，等待完成）
static void ST7735_WriteData_DMA(uint8_t *buff, size_t buff_size)
{
  dma_transfer_complete = 0;
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, buff, buff_size);
  
  // 等待DMA传输完成
  while (!dma_transfer_complete);
}

// SPI DMA传输完成回调
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1)
  {
    dma_transfer_complete = 1;
  }
}

static void ST7735_ExecuteCommandList(const uint8_t *addr)
{
  uint8_t numCommands, numArgs;
  uint16_t ms;

  numCommands = *addr++;
  while (numCommands--)
  {
    uint8_t cmd = *addr++;
    ST7735_WriteCommand(cmd);

    numArgs = *addr++;
    // If high bit set, delay follows args
    ms = numArgs & DELAY;
    numArgs &= ~DELAY;
    if (numArgs)
    {
      ST7735_WriteData((uint8_t *)addr, numArgs);
      addr += numArgs;
    }

    if (ms)
    {
      ms = *addr++;
      if (ms == 255)
        ms = 500;
      HAL_Delay(ms);
    }
  }
}

static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
  // column address set
  ST7735_WriteCommand(ST7735_CASET);
  uint8_t data[] = {0x00, static_cast<uint8_t>(x0 + ST7735_XSTART), 0x00, static_cast<uint8_t>(x1 + ST7735_XSTART)};
  ST7735_WriteData(data, sizeof(data));

  // row address set
  ST7735_WriteCommand(ST7735_RASET);
  data[1] = static_cast<uint8_t>(y0 + ST7735_YSTART);
  data[3] = static_cast<uint8_t>(y1 + ST7735_YSTART);
  ST7735_WriteData(data, sizeof(data));

  // write to RAM
  ST7735_WriteCommand(ST7735_RAMWR);
}

void ST7735_Init()
{
  ST7735_Select();
  ST7735_Reset();
  ST7735_ExecuteCommandList(init_cmds1);
  ST7735_ExecuteCommandList(init_cmds2);
  ST7735_ExecuteCommandList(init_cmds3);
  ST7735_Unselect();
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;

  ST7735_Select();

  ST7735_SetAddressWindow(x, y, x + 1, y + 1);
  uint8_t data[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  ST7735_WriteData(data, sizeof(data));

  ST7735_Unselect();
}

static void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
  uint32_t i, b, j;

  ST7735_SetAddressWindow(x, y, x + font.width - 1, y + font.height - 1);

  for (i = 0; i < font.height; i++)
  {
    b = font.data[(ch - 32) * font.height + i];
    for (j = 0; j < font.width; j++)
    {
      if ((b << j) & 0x8000)
      {
        uint8_t data[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
        ST7735_WriteData(data, sizeof(data));
      }
      else
      {
        uint8_t data[] = {static_cast<uint8_t>(bgcolor >> 8), static_cast<uint8_t>(bgcolor & 0xFF)};
        ST7735_WriteData(data, sizeof(data));
      }
    }
  }
}

/*
Simpler (and probably slower) implementation:

static void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color) {
    uint32_t i, b, j;

    for(i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for(j = 0; j < font.width; j++) {
            if((b << j) & 0x8000)  {
                ST7735_DrawPixel(x + j, y + i, color);
            }
        }
    }
}
*/

void ST7735_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{
  ST7735_Select();

  while (*str)
  {
    if (x + font.width >= ST7735_WIDTH)
    {
      x = 0;
      y += font.height;
      if (y + font.height >= ST7735_HEIGHT)
      {
        break;
      }

      if (*str == ' ')
      {
        // skip spaces in the beginning of the new line
        str++;
        continue;
      }
    }

    ST7735_WriteChar(x, y, *str, font, color, bgcolor);
    x += font.width;
    str++;
  }

  ST7735_Unselect();
}

void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // clipping
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    w = ST7735_WIDTH - x;
  if ((y + h - 1) >= ST7735_HEIGHT)
    h = ST7735_HEIGHT - y;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  uint8_t data[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  for (y = h; y > 0; y--)
  {
    for (x = w; x > 0; x--)
    {
      HAL_SPI_Transmit(&ST7735_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
    }
  }

  ST7735_Unselect();
}

void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // clipping
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    w = ST7735_WIDTH - x;
  if ((y + h - 1) >= ST7735_HEIGHT)
    h = ST7735_HEIGHT - y;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  // Prepare whole line in a single buffer
  uint8_t pixel[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  uint8_t *line = static_cast<uint8_t*>(malloc(w * sizeof(pixel)));
  for (x = 0; x < w; ++x)
    memcpy(line + x * sizeof(pixel), pixel, sizeof(pixel));

  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  for (y = h; y > 0; y--)
    HAL_SPI_Transmit(&ST7735_SPI_PORT, line, w * sizeof(pixel), HAL_MAX_DELAY);

  free(line);
  ST7735_Unselect();
}

// DMA版本 - 最高性能
void ST7735_FillRectangle_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // clipping
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    w = ST7735_WIDTH - x;
  if ((y + h - 1) >= ST7735_HEIGHT)
    h = ST7735_HEIGHT - y;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  // 准备整行数据缓冲区
  uint8_t pixel[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  uint8_t *line = static_cast<uint8_t*>(malloc(w * sizeof(pixel)));
  for (x = 0; x < w; ++x)
    memcpy(line + x * sizeof(pixel), pixel, sizeof(pixel));

  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  
  // 使用DMA传输每一行
  for (y = h; y > 0; y--)
  {
    ST7735_WriteData_DMA(line, w * sizeof(pixel));
  }

  free(line);
  ST7735_Unselect();
}

// DMA整屏填充 - 最快速度
void ST7735_FillScreen_DMA(uint16_t color)
{
  ST7735_FillRectangle_DMA(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

// DMA图像绘制 - 高速图像传输
void ST7735_DrawImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    return;
  if ((y + h - 1) >= ST7735_HEIGHT)
    return;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
  
  // 使用DMA传输整个图像
  ST7735_WriteData_DMA((uint8_t *)data, sizeof(uint16_t) * w * h);
  
  ST7735_Unselect();
}

void ST7735_FillScreen(uint16_t color)
{
  ST7735_FillRectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_FillScreenFast(uint16_t color)
{
  ST7735_FillRectangleFast(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    return;
  if ((y + h - 1) >= ST7735_HEIGHT)
    return;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
  ST7735_WriteData((uint8_t *)data, sizeof(uint16_t) * w * h);
  ST7735_Unselect();
}

void ST7735_InvertColors(bool invert)
{
  ST7735_Select();
  ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
  ST7735_Unselect();
}

void ST7735_SetGamma(uint8_t gamma)
{
  ST7735_Select();
  ST7735_WriteCommand(ST7735_GAMSET);
  ST7735_WriteData(&gamma, sizeof(gamma));
  ST7735_Unselect();
}

void ST7735_Print(uint16_t x, uint16_t y, FontDef font, uint16_t color, uint16_t bgcolor, const char *format, ...)
{
  char temp[256];
  va_list ap;
  va_start(ap, format);
  vsprintf(temp, format, ap);
  va_end(ap);
  ST7735_WriteString(x, y, temp, font, color, bgcolor);
}

/*============================================================================
 * 图标绘制函数
 *============================================================================*/

void ST7735_DrawIcon(uint16_t x, uint16_t y, IconIndex icon)
{
  if (icon >= ICON_COUNT) return;
  if ((x + ICON_WIDTH > ST7735_WIDTH) || (y + ICON_HEIGHT > ST7735_HEIGHT)) return;
  
  ST7735_DrawImage(x, y, ICON_WIDTH, ICON_HEIGHT, Icons[icon].data);
}

/*============================================================================
 * 辅助图形绘制函数
 *============================================================================*/

void ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
  if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
  
  ST7735_FillRectangle(x, y, w, 1, color);
}

void ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
  if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
  
  ST7735_FillRectangle(x, y, 1, h, color);
}

void ST7735_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  ST7735_DrawHLine(x, y, w, color);
  ST7735_DrawHLine(x, y + h - 1, w, color);
  ST7735_DrawVLine(x, y, h, color);
  ST7735_DrawVLine(x + w - 1, y, h, color);
}

void ST7735_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color)
{
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  
  // Draw straight lines
  ST7735_DrawHLine(x + r, y, w - 2 * r, color);
  ST7735_DrawHLine(x + r, y + h - 1, w - 2 * r, color);
  ST7735_DrawVLine(x, y + r, h - 2 * r, color);
  ST7735_DrawVLine(x + w - 1, y + r, h - 2 * r, color);
  
  // Draw corners (simple approximation)
  int16_t i;
  for (i = 0; i <= r; i++)
  {
    int16_t dx = (int16_t)(r * 0.7f);
    if (i < r)
    {
      // Top-left corner
      ST7735_DrawPixel(x + r - i, y + r - dx, color);
      ST7735_DrawPixel(x + r - dx, y + r - i, color);
      // Top-right corner
      ST7735_DrawPixel(x + w - 1 - r + i, y + r - dx, color);
      ST7735_DrawPixel(x + w - 1 - r + dx, y + r - i, color);
      // Bottom-left corner
      ST7735_DrawPixel(x + r - i, y + h - 1 - r + dx, color);
      ST7735_DrawPixel(x + r - dx, y + h - 1 - r + i, color);
      // Bottom-right corner
      ST7735_DrawPixel(x + w - 1 - r + i, y + h - 1 - r + dx, color);
      ST7735_DrawPixel(x + w - 1 - r + dx, y + h - 1 - r + i, color);
    }
  }
}

void ST7735_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color)
{
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  
  // Fill main rectangle
  ST7735_FillRectangle(x + r, y, w - 2 * r, h, color);
  ST7735_FillRectangle(x, y + r, r, h - 2 * r, color);
  ST7735_FillRectangle(x + w - r, y + r, r, h - 2 * r, color);
  
  // Fill corners
  int16_t i, j;
  for (i = 0; i < r; i++)
  {
    for (j = 0; j < r; j++)
    {
      if ((i - r) * (i - r) + (j - r) * (j - r) <= r * r)
      {
        // Top-left
        ST7735_DrawPixel(x + i, y + j, color);
        // Top-right
        ST7735_DrawPixel(x + w - 1 - i, y + j, color);
        // Bottom-left
        ST7735_DrawPixel(x + i, y + h - 1 - j, color);
        // Bottom-right
        ST7735_DrawPixel(x + w - 1 - i, y + h - 1 - j, color);
      }
    }
  }
}

/*============================================================================
 * 美化显示函数 - 带图标的数值显示
 *============================================================================*/

void ST7735_DrawValueWithIcon(uint16_t x, uint16_t y, IconIndex icon, 
                               const char *value, const char *unit,
                               uint16_t valueColor, uint16_t unitColor)
{
  // Draw icon
  ST7735_DrawIcon(x, y, icon);
  
  // Draw value
  ST7735_WriteString(x + ICON_WIDTH + 2, y + 1, value, Font_7x10, valueColor, ST7735_BLACK);
  
  // Draw unit (smaller, after value)
  uint8_t valueLen = 0;
  while (value[valueLen]) valueLen++;
  ST7735_WriteString(x + ICON_WIDTH + 2 + valueLen * 7, y + 1, unit, Font_7x10, unitColor, ST7735_BLACK);
}
