# AGENTS.md - LoveFinder776-3LaneINA3221Dev

## 项目概述

嵌入式固件项目，STM32G030F6Px (Cortex-M0+) 微控制器，三路 INA3221 电压/电流监测开发板。

## 硬件配置

- **MCU**: STM32G030F6Px (TSSOP20, 32KB Flash, 8KB RAM)
- **Clock**: HSI 16MHz → PLL → 64MHz SYSCLK
- **Peripherals**: I2C1 (PB8/PB9), SPI1 (PA5/PA7), USART2 (PA2/PA3), ADC1 (TempSensor), TIM3, DMA1_Channel1 (SPI1_TX)
- **GPIO**: PA0=LED1, PA1=KEY1, PA4=CS, PA6=DC, PA8=RESET, PA11=LCD_EN, PA12=INASS_ERR, PB6=INASS_PV, PC15=INASS_WARN

## 构建工具

- **Toolchain**: Keil MDK-ARM V5.32 with ARMCLANG 6.24 (AC6)
- **IDE Config**: `MDK-ARM/LoveFinder776-3LaneINA3221Dev.uvprojx`
- **Project Config**: `LoveFinder776-3LaneINA3221Dev.ioc` (STM32CubeMX 6.17)
- **Startup**: `MDK-ARM/startup_stm32g030xx.s`
- **Flash**: 0x8000000 (32KB), **RAM**: 0x20000000 (8KB)

## 源码结构

```
Core/Src/main.cpp          # 入口，main()，应用逻辑
Core/Inc/main.h            # Pin定义 (LED1, KEY1, LCD_*, INASS_*)
Core/Src/gpio.c/i2c.c/     # HAL外设初始化 (CubeMX生成)
Core/Src/spi.c/usart.c/
Core/Src/adc.c/tim.c/dma.c
Core/Src/stm32g0xx_it.c    # ISR向量表
Core/Src/stm32g0xx_hal_msp.c
Drivers/                   # STM32G0xx_HAL_Driver + CMSIS (CubeMX生成)
LoveFinderLib/             # 自定义驱动库 (C++17)
  AT24C04/                 # EEPROM驱动
  BUTTON/                  # 按键驱动 (状态机轮询)
  EEPROM/                  # EEPROM高层封装
  FontLib/                 # 字库模块 (font_manifest.json 唯一真相源，PickSoul 兼容)
    font_manifest.json     # 机器可读的真相源 (PickSoul 读写)
    font.h                 # FontLib 公共接口 (FontDef + 查表声明)
    font_data.h            # 子集字体 extern 声明
    font_data.cpp          # 子集字体位图数据 (字符级编译, 全 C++)
    font_config.hpp        # 字体编译控制 (USE_FONT_SUBSET_*, CHARS 宏)
    fonts.cpp              # 完整字库 (模式A, 当前无完整字体, 仅占位)
  I2C/                     # I2C扫描驱动
  INA3221/                 # (目录空，TODO)
  ST7735/                  # LCD驱动 + icons (无字体文件，字体已迁移到 FontLib)
```

## 关键依赖

- **HAL**: `stm32g0xx_hal.h` (CubeMX生成，启用HAL_ASSERT)
- **C++**: C++17，main.cpp 后缀 `.cpp`

## 应用逻辑

- 启动：LED闪3次，初始化LCD + EEPROM
- 上电：随机写一次EEPROM
- **单击 (CLICK)**: 随机写EEPROM
- **双击 (DOUBLE_CLICK)**: 读验证，显示PASS/FAIL
- **长按 (LONG_PRESS)**: 格式化EEPROM
- **注意**: `m_pendingClickValid` 在双击/长按释放后必须清除，防止状态残留

## 构建命令

- **Keil uVision**: 打开 `.uvprojx`，Build (F7)
- 编译产物输出到 `MDK-ARM/LoveFinder776-3LaneINA3221Dev/`

## 注意事项

- CubeMX配置在 `.ioc` 中，修改后重新生成代码会覆盖 `Core/Src/*.c` 和 `Core/Inc/*.h`
- `main.cpp` 位于 `Core/Src/`，不在 `LoveFinderLib/`，可直接编辑不受CubeMX影响
- 所有自定义库在 `LoveFinderLib/`，用C++编写，main.cpp 调用
- LCD使用SPI1 (PA5=SCK, PA7=MOSI)，片选PA4，DC PA6，复位PA8，使能PA11