# LoveFinderSeries NO.776 — 三通道电压电流测量仪

基于 **INA3221 + STM32G0** 的经济型三通道电压/电流测量仪（软硬件全部开源）。

## 核心功能

- **电压电流监测**：三通道独立实时监测，160×80 彩色 LCD 分栏显示
- **电量统计**：软件库仑计，以 200ms 周期对输入通道电流积分，累计 mAh 电量

## 关键元件

| 元件 | 型号 | 说明 |
|---|---|---|
| MCU | STM32G030F6P6 | Cortex-M0+，TSSOP20，32KB Flash / 8KB RAM，PLL 至 64MHz |
| 监测芯片 | INA3221 | 三通道电压/电流监测，I2C 接口（地址 0x41，A0=VS），5mΩ 采样电阻 |
| 显示屏 | ST7735 | 160×80 SPI 彩色 LCD |
| 板上 3V3 | JW5026 | 耐压非常高（祖传方案），RY8411 可 P2P 替代 |
| DC 接口 | DC-044 ×3 | 壳体高度限制下的紧凑座子，单路电流建议 ≤5A |

## 显示界面

- 状态栏：输入电压
- 功率行：总功率 + 累计电量（mAh）
- 三列：每通道电流（XX.YY A）、单通道功率（XXX.X W）、进度条
- 输入源自动识别：CH1/CH2 负电流检测（外部电源灌入），CH3 板载 3.3V 电源口兜底
- 按键（KEY1）：长按进入 I2C 总线设备扫描显示

## 成本与复刻

- INA3221 单颗 **2 元以内**，STM32G030F6P6 少量购买 **2 元以内**
- 整机成本 **不超过 20 元**
- 外壳可在 **JLCFA** 购买；前后面板均为 PCB 文件，屏幕也有独立 PCB 文件
- 复刻需打样 **4 张 PCB**（主板 + 前面板 + 后面板 + 屏幕），均为 2 层板，可在 JLC 免费打样

## 软件架构

- **语言/标准**：C++17（无 C API 兼容层，全部 C++）
- **工具链**：Keil MDK-ARM V5.32 / ARMCLANG 6.24 (AC6)，工程文件 `MDK-ARM/LoveFinder776-3LaneINA3221Dev.uvprojx`
- **CubeMX 配置**：`LoveFinder776-3LaneINA3221Dev.ioc`（STM32CubeMX 6.17）

### 源码结构

```
Core/Src/main.cpp          # 入口与应用逻辑（显示、库仑计、输入源识别）
Core/Inc/main.h            # 引脚定义
Core/Src/*.c               # HAL 外设初始化（CubeMX 生成）
Drivers/                   # STM32G0xx_HAL_Driver + CMSIS
LoveFinderLib/             # 自定义 C++ 驱动库
  AT24C04/                 # EEPROM 驱动
  BUTTON/                  # 按键状态机（单击/双击/长按）
  EEPROM/                  # EEPROM 高层封装
  FontLib/                 # 字库模块（font_manifest.json 为唯一真相源，PickSoul 兼容）
  I2C/                     # I2C 总线驱动（含扫描）
  INA3221/                 # 三通道电压/电流监测驱动
  ST7735/                  # LCD 驱动 + 图标
```

### 引脚分配（STM32G030F6Px）

| 引脚 | 功能 |
|---|---|
| PA0 | LED1 |
| PA1 | KEY1（低电平有效） |
| PA4 | LCD CS |
| PA5 / PA7 | SPI1 SCK / MOSI |
| PA6 | LCD DC |
| PA8 | LCD RESET |
| PA11 | LCD_EN |
| PA12 | INASS_ERR |
| PA2 / PA3 | USART2 |
| PB6 | INASS_PV |
| PB8 / PB9 | I2C1 SCL / SDA（INA3221、EEPROM） |
| PC15 | INASS_WARN |

## 构建

打开 `MDK-ARM/LoveFinder776-3LaneINA3221Dev.uvprojx`，Build（F7）即可。命令行：

```
UV4.exe -b MDK-ARM/LoveFinder776-3LaneINA3221Dev.uvprojx -j0
```

## 开源

软件项目开源在 GitHub。
