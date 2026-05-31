# LoRa 裁判记分系统重设计方案

## 1. 目标

当前 WiFi 版本使用 ESP32 服务端开启热点，裁判端通过 WiFi 直接提交比分。在室内有墙、人群、设备遮挡的 30 到 80 米环境下，ESP32 WiFi 稳定性不足。

新方案目标：

- 保留“裁判机 + 服务端机器”的简洁设备结构，不增加独立网关。
- 裁判端通信从 WiFi 改为 470MHz LoRa。
- 服务端继续提供 WiFi AP 和网页显示/控制。
- 保留旧项目 `esp32-client` 和 `score_system` 不动，新建 LoRa 版本项目。
- 第一版先做 1 台裁判机 + 1 台服务端，验证稳定后再扩展到 3 台裁判机。

## 2. 总体架构

```text
裁判机 x3
ESP32-S3 + E22-400T22D LoRa UART + TM1637 + 5按键 + 低电量LED + 18650

        LoRa 470MHz / UART透明传输 / CSV+CRC16

服务端 x1
ESP32-S3 + E22-400T22D/T30D + WiFi AP + WebServer网页 + NVS状态保存 + 2实体按钮 + 4状态灯

        WiFi AP

手机/电脑网页
显示总比分、队伍名称、倒计时；控制页管理裁判、队伍名称、下一轮/重置
```

## 3. 硬件选型

### 3.1 主控

- 裁判机：ESP32-S3 开发板。
- 服务端：ESP32-S3 开发板。

统一使用 ESP32-S3，方便共用底层代码，也能提供足够 GPIO 给 LoRa、按键、TM1637、电池检测和实体控制按钮。

### 3.2 LoRa 模块

第一版使用 UART 透明传输模块，并优先购买插针版，便于洞洞板和杜邦线原型验证：

- 裁判机：E22-400T22D，22dBm，UART，410.125~493.125MHz，插针/半孔板，约 21mm x 36mm。
- 服务端：E22-400T22D 起步，必要时可换 E22-400T30D。
- 不建议第一版直接购买 `T22S` 贴片版，除非同时购买配套转接板或测试底板。

频段选择：

- 国内使用，按 CN470 / 470MHz 方向采购。
- 不采购 915MHz 美版模块。
- 不采购 2.4GHz LoRa 模块。

### 3.3 天线

- 裁判机和服务端都使用外置 SMA 470MHz 天线。
- 如果购买 `E22-400T22D`，通常模块自带 SMA-K 天线座，直接购买 SMA-J 公头胶棒天线即可。
- 如果购买 `T22S` 贴片版，需要确认是 IPEX 还是邮票孔天线形式，并额外购买转接线/转接板。
- 裁判机天线放在顶部，远离 18650、电源模块和手握区域。
- 服务端天线竖直放高，避免贴地或靠近金属桌面。

### 3.4 裁判机显示与按键

保留现有交互形态：

- 5 个按键。
- 1 个 TM1637 四位数码管。
- 1 个低电量指示 LED。

按键语义：

```text
红 +1：短按红 +1 键
红 -1：长按红 +1 键
红 +2：短按红 +2 键
红 -2：长按红 +2 键
蓝 +1：短按蓝 +1 键
蓝 -1：长按蓝 +1 键
蓝 +2：短按蓝 +2 键
蓝 -2：长按蓝 +2 键
提交：短按提交键
本轮清零：长按提交键
```

分数下限为 0，不能减成负数。

### 3.5 电池与供电

裁判机：

- 单节 18650 电池。
- 允许取出 18650 单独充电。
- 不在第一版内置充电模块。
- 机械电源开关。
- 推荐第一版使用 18650 升压到 5V 的电源模块，输出持续 >= 1A，峰值 >= 2A。
- ESP32-S3 开发板从 5V/VIN 供电。
- E22-400T22D 和 TM1637 推荐统一使用 3.3V 供电，避免 5V 逻辑电平进入 ESP32-S3 GPIO。
- 3.3V 可来自 ESP32-S3 开发板 3V3 引脚；如果发射时不稳定，则改用独立 3.3V 稳压模块给 E22 和 TM1637 供电。
- 电池电压分压接入 ESP32 ADC。
- E22 VCC 附近放 470uF 电容 + 0.1uF 陶瓷电容。
- ESP32 3.3V 附近放 100uF 电容 + 0.1uF 陶瓷电容。

服务端：

- 5V/2A USB 供电。
- 如果开发板 3.3V 供电余量不足，E22 单独使用 3.3V 稳压。
- E22 VCC 附近同样放 470uF + 0.1uF。

### 3.6 电容接线方式

电容用于缓冲 LoRa 发射瞬间电流和抑制高频噪声。第一版使用洞洞板、排线或杜邦线时，电源线阻抗较高，更应该在模块附近加电容。

#### 裁判机推荐接法

```text
18650 正极
  -> 电源开关
  -> 5V 升压模块 IN+

18650 负极
  -> 5V 升压模块 IN-
  -> 系统 GND

5V 升压模块 OUT+
  -> ESP32-S3 开发板 5V/VIN
  -> 可选独立 3.3V 稳压模块 IN+

5V 升压模块 OUT-
  -> ESP32-S3 GND
  -> 可选独立 3.3V 稳压模块 IN-

ESP32-S3 3V3 或独立 3.3V 稳压模块 OUT+
  -> E22 VCC
  -> TM1637 VCC

ESP32-S3 GND 或独立 3.3V 稳压模块 OUT-
  -> E22 GND
  -> TM1637 GND
```

在 E22 模块供电脚附近并联：

```text
E22 VCC ----+---- 470uF 电解/钽电容正极
            |
            +---- 0.1uF 陶瓷电容任意一脚

E22 GND ----+---- 470uF 电解/钽电容负极
            |
            +---- 0.1uF 陶瓷电容另一脚
```

在 ESP32-S3 供电脚附近并联：

```text
ESP32 3V3 ----+---- 100uF 电解/钽电容正极
              |
              +---- 0.1uF 陶瓷电容任意一脚

ESP32 GND ----+---- 100uF 电解/钽电容负极
              |
              +---- 0.1uF 陶瓷电容另一脚
```

在 5V 升压模块输出端附近并联：

```text
升压 OUT+ ---- 220uF 或 470uF 电解电容正极
升压 OUT- ---- 220uF 或 470uF 电解电容负极
```

#### 服务端推荐接法

如果 E22 直接从 ESP32-S3 开发板 3.3V 取电：

```text
ESP32 3V3 -> E22 VCC
ESP32 GND -> E22 GND

E22 VCC-GND 旁并联 470uF + 0.1uF
```

如果 E22 使用独立 3.3V 稳压模块：

```text
5V USB 输入 -> 独立 3.3V 稳压模块 IN
独立稳压 OUT+ -> E22 VCC
独立稳压 OUT- -> E22 GND
ESP32 GND 与独立稳压 OUT- 必须共地

E22 VCC-GND 旁并联 470uF + 0.1uF
独立稳压 OUT+ / OUT- 旁并联 220uF 或 470uF
```

#### 极性和摆放要求

- 电解电容、钽电容有正负极，正极接对应电源正极，负极接 GND，不能接反。
- 陶瓷电容通常无极性，任意方向都可以。
- 电容要尽量靠近被保护的模块，尤其是 E22 的 VCC/GND。
- 连接线越短越好，不要把电容放在离模块很远的位置。
- 470uF 可选耐压 6.3V 或更高；如果采购方便，选 10V 更稳妥。
- 0.1uF 陶瓷电容建议选 X7R 或 X5R，常见 50V 规格即可。

## 4. 裁判机接线表

避开 ESP32-S3 启动相关引脚，例如 GPIO0、GPIO3、GPIO45、GPIO46。

不同 ESP32-S3 开发板的物理排针顺序可能不同，接线时看开发板丝印的 GPIO 名称，不按“第几针”接。

裁判端按常见 ESP32-S3-DevKitC-1 排针布局规划走线：

- E22-400T22D 使用右侧连续 GPIO38/GPIO39/GPIO40/GPIO41/GPIO42，整组靠近 LoRa 模块走线。
- TM1637 用 GPIO48/GPIO47，5 个按键 GPIO5/GPIO4/GPIO13/GPIO14/GPIO8，电池 ADC GPIO15，低电量 LED GPIO2。
- GPIO35~GPIO37 不接任何外设：N16R8 等带 OPI Flash/PSRAM 的 ESP32-S3 用这三个脚连八线 PSRAM，占用后不能复用。

### 4.1 裁判机总接线图

```text
18650 电池
  -> 电源开关
  -> 5V 升压模块
  -> ESP32-S3 开发板 5V/VIN

ESP32-S3 3V3 或独立 3.3V 稳压模块
  -> E22-400T22D VCC
  -> TM1637 VCC

所有 GND 必须共地：
18650 负极 / 升压模块 GND / ESP32 GND / E22 GND / TM1637 GND / 按键 GND
```

### 4.2 裁判机电源模块接线

| 模块引脚 | 接到哪里 | 说明 |
|---|---|---|
| 18650 电池座红线 `BAT+` | 电源开关输入脚 | 切断电池正极 |
| 电源开关输出脚 | 5V 升压模块 `IN+` | 开关打开后给升压模块供电 |
| 18650 电池座黑线 `BAT-` | 5V 升压模块 `IN-` / 系统 GND | 负极共地 |
| 5V 升压模块 `OUT+` | ESP32-S3 `5V` 或 `VIN` | 给开发板供电 |
| 5V 升压模块 `OUT-` | ESP32-S3 `GND` | 与系统 GND 相连 |
| 5V 升压模块 `OUT+` | 可选 3.3V 稳压模块 `IN+` | 如果不用开发板 3V3 给 E22 供电，则接这里 |
| 5V 升压模块 `OUT-` | 可选 3.3V 稳压模块 `IN-` | 与系统 GND 相连 |
| 3.3V 稳压模块 `OUT+` | E22 `VCC`、TM1637 `VCC` | 推荐给逻辑模块供 3.3V |
| 3.3V 稳压模块 `OUT-` | E22 `GND`、TM1637 `GND` | 与系统 GND 相连 |

如果暂时不加独立 3.3V 稳压模块，则：

```text
ESP32-S3 3V3 -> E22 VCC
ESP32-S3 3V3 -> TM1637 VCC
ESP32-S3 GND -> E22 GND / TM1637 GND
```

如果 E22 发射时 ESP32 重启、显示闪烁或 ACK 丢失，再改为独立 3.3V 稳压模块供 E22。

### 4.3 E22-400T22D 接线

按模块丝印接线，不同批次模块排针顺序可能不同。

| E22-400T22D 引脚 | 接到 ESP32-S3 / 电源 | 方向 | 程序配置 | 说明 |
|---|---|---|---|---|
| `VCC` | 3.3V | 电源输入 | 无 | 推荐 3.3V 供电，避免串口 5V 电平风险 |
| `GND` | GND | 电源地 | 无 | 必须与 ESP32 共地 |
| `TXD` | ESP32 `GPIO41` | E22 -> ESP32 | `Serial1 RX` | 模块发送数据到 ESP32 |
| `RXD` | ESP32 `GPIO40` | ESP32 -> E22 | `Serial1 TX` | ESP32 发送数据到模块 |
| `AUX` | ESP32 `GPIO42` | E22 -> ESP32 | `INPUT_PULLUP` | 判断模块忙闲，可用于发送前等待 |
| `M0` | ESP32 `GPIO38` | ESP32 -> E22 | `OUTPUT`，默认 `LOW` | 模式选择脚，不能悬空 |
| `M1` | ESP32 `GPIO39` | ESP32 -> E22 | `OUTPUT`，默认 `LOW` | 模式选择脚，不能悬空 |
| `RST`/`NRST` | 默认不接 | ESP32 -> E22 | 当前固件未使用 | 只有模块带复位脚时才考虑接；没有该脚则忽略 |

正常透传模式：

```text
M0 = LOW
M1 = LOW
```

如果第一版暂时不想用 GPIO 控制 M0/M1，也可以：

```text
E22 M0 -> GND
E22 M1 -> GND
```

但推荐接到 GPIO，后续配置模块参数更方便。

### 4.4 TM1637 数码管接线

| TM1637 模块引脚 | 接到 ESP32-S3 / 电源 | 说明 |
|---|---|---|
| `VCC` | 3.3V | 推荐 3.3V 供电，避免 DIO/CLK 上拉到 5V |
| `GND` | GND | 与 ESP32 共地 |
| `CLK` | ESP32 `GPIO48` | 时钟线 |
| `DIO` | ESP32 `GPIO47` | 数据线 |

如果某个 TM1637 模块在 3.3V 下亮度不足，再考虑换模块或做电平转换，不建议直接把 VCC 接 5V 后把 DIO/CLK 直接连 ESP32。

### 4.5 五个按键接线

所有按键使用 `INPUT_PULLUP`，按下时 GPIO 被拉到 GND。

| 功能 | ESP32-S3 GPIO | 按键另一端 | 程序动作 |
|---|---|---|---|
| 红 +1 / 红 -1 | `GPIO5` | GND | 短按 +1，长按 -1 |
| 红 +2 / 红 -2 | `GPIO4` | GND | 短按 +2，长按 -2 |
| 蓝 +1 / 蓝 -1 | `GPIO13` | GND | 短按 +1，长按 -1 |
| 蓝 +2 / 蓝 -2 | `GPIO14` | GND | 短按 +2，长按 -2 |
| 提交 / 清零 | `GPIO8` | GND | 短按提交，长按清零 |

四脚轻触开关内部是“两边各一组常通”。接线前用万用表蜂鸣档确认，选择开关按下后才导通的两侧：

```text
GPIO5  ---- 红 +1 按键 ---- GND
GPIO4  ---- 红 +2 按键 ---- GND
GPIO13 ---- 蓝 +1 按键 ---- GND
GPIO14 ---- 蓝 +2 按键 ---- GND
GPIO8  ---- 提交按键 ---- GND
```

### 4.6 电池电压检测接线

使用两个 100K 电阻分压，检测原始 18650 电压，不检测升压后的 5V。

```text
电源开关输出后的 BAT+ ---- 100K ----+---- GPIO15 ADC
                                      |
                                      +---- 0.1uF 陶瓷电容 ---- GND，可选
                                      |
GND ---------------------- 100K ------+
```

换算关系：

```text
ADC 节点电压 = 电池电压 / 2
满电 4.2V -> ADC 约 2.1V
```

注意：

- 分压上端接电源开关后的电池正极，关机时不耗电。
- ADC 输入不要直接接 18650 正极。
- GPIO15 必须配置为 ADC 输入，不要同时接其他模块。

### 4.7 裁判机低电量 LED 接线

电量偏低时由固件闪烁该 LED 提示（不再用数码管显示 `Lo.B`）。LED 必须串联限流电阻，高电平点亮。

| 功能 | ESP32-S3 GPIO | 接线 |
|---|---|---|
| 低电量灯 | `GPIO2` | GPIO2 -> 1K 电阻 -> LED 正极，LED 负极 -> GND |

### 4.8 裁判机电容接线汇总

| 电容 | 正极/一端 | 负极/另一端 | 放置位置 |
|---|---|---|---|
| 470uF 电解 | E22 `VCC` | E22 `GND` | 尽量靠近 E22 |
| 0.1uF 陶瓷 | E22 `VCC` | E22 `GND` | 尽量靠近 E22 |
| 220uF 或 470uF 电解 | 5V 升压 `OUT+` | 5V 升压 `OUT-` | 靠近升压模块输出 |
| 100uF 电解 | ESP32 `3V3` | ESP32 `GND` | 靠近 ESP32 供电脚 |
| 0.1uF 陶瓷 | ESP32 `3V3` | ESP32 `GND` | 靠近 ESP32 供电脚 |
| 0.1uF 陶瓷，可选 | GPIO15 ADC 节点 | GND | 靠近 ESP32 ADC 引脚 |

## 5. 服务端接线表

服务端同样按开发板丝印 GPIO 名称接线。

### 5.1 服务端总接线图

```text
5V/2A USB 电源
  -> ESP32-S3 开发板 USB-C

ESP32-S3 3V3 或独立 3.3V 稳压模块
  -> E22-400T22D VCC

所有 GND 必须共地：
ESP32 GND / E22 GND / 按键 GND / LED GND
```

服务端 LoRa 接线与裁判端保持同一套 GPIO 映射：

- E22-400T22D 使用右侧连续 GPIO38/GPIO39/GPIO40/GPIO41/GPIO42，便于两块洞洞板复用同一走线模板。
- 服务端实体按钮 GPIO5/GPIO12；状态灯电源 GPIO45、工作 GPIO48、TX GPIO47、RX GPIO21；服务端不接数码管。
- GPIO35~GPIO37 不接任何外设：N16R8 等带 OPI Flash/PSRAM 的 ESP32-S3 用这三个脚连八线 PSRAM，占用后不能复用。

### 5.2 服务端 E22-400T22D 接线

| E22-400T22D 引脚 | 接到 ESP32-S3 / 电源 | 方向 | 程序配置 | 说明 |
|---|---|---|---|---|
| `VCC` | 3.3V | 电源输入 | 无 | T22D 可先用 ESP32 3V3，若不稳再用独立 3.3V |
| `GND` | GND | 电源地 | 无 | 必须与 ESP32 共地 |
| `TXD` | ESP32 `GPIO41` | E22 -> ESP32 | `Serial1 RX` | 服务端接收裁判机数据 |
| `RXD` | ESP32 `GPIO40` | ESP32 -> E22 | `Serial1 TX` | 服务端发送 ACK/STATUS |
| `AUX` | ESP32 `GPIO42` | E22 -> ESP32 | `INPUT_PULLUP` | 判断模块忙闲 |
| `M0` | ESP32 `GPIO38` | ESP32 -> E22 | `OUTPUT`，默认 `LOW` | 正常透传模式 |
| `M1` | ESP32 `GPIO39` | ESP32 -> E22 | `OUTPUT`，默认 `LOW` | 正常透传模式 |
| `RST`/`NRST` | 默认不接 | ESP32 -> E22 | 当前固件未使用 | 只有模块带复位脚时才考虑接 |

### 5.3 服务端实体按钮接线

| 功能 | ESP32-S3 GPIO | 按键另一端 | 程序动作 |
|---|---|---|---|
| 下一轮 | `GPIO5` | GND | 短按；仅本轮完成后有效 |
| 重置总分 | `GPIO12` | GND | 长按 3 秒后重置总比分 |

按键同样使用 `INPUT_PULLUP`：

```text
GPIO5  ---- 下一轮按键 ---- GND
GPIO12 ---- 重置按键 ---- GND
```

第一版现状：下一轮按短按按下沿触发，暂未强制"本轮完成才允许"（完成判定属步骤 14）；重置长按 3 秒清空当前轮次与比分状态、不动设备绑定，步骤 14 接入累计总分后在同一处一并清零。也提供等价的 `next-round` / `reset` 串口命令便于无按钮调试。

### 5.4 服务端状态灯接线

4 个状态灯，每个 LED 必须串联限流电阻，高电平点亮。

| 功能 | ESP32-S3 GPIO | 行为 | 接线 |
|---|---|---|---|
| 电源灯 | `GPIO45` | 上电常亮 | GPIO45 -> 1K 电阻 -> LED 正极，LED 负极 -> GND |
| 工作灯 | `GPIO48` | 1Hz 心跳慢闪，表示固件在运行 | GPIO48 -> 1K 电阻 -> LED 正极，LED 负极 -> GND |
| 发送指示 TX | `GPIO47` | 每发一帧闪一下 | GPIO47 -> 1K 电阻 -> LED 正极，LED 负极 -> GND |
| 接收指示 RX | `GPIO21` | 每收到一帧闪一下 | GPIO21 -> 1K 电阻 -> LED 正极，LED 负极 -> GND |

注意：

- `GPIO45` 是 ESP32-S3 strapping 脚（VDD_SPI 电压选择），复位瞬间必须为低 = 3.3V flash。上面这种"高电平点亮（LED 接到 GND）"开机时引脚被拉低正好满足；**切勿**改成"低电平点亮（LED 接到 3V3）"，否则复位时把 VDD_SPI 拉成 1.8V 会导致无法启动。
- `GPIO48` 在部分 ESP32-S3-DevKitC-1 上是板载 RGB 灯，用作工作灯时板载灯会跟着闪，不影响功能。

### 5.5 服务端电容接线汇总

| 电容 | 正极/一端 | 负极/另一端 | 放置位置 |
|---|---|---|---|
| 470uF 电解 | E22 `VCC` | E22 `GND` | 尽量靠近 E22 |
| 0.1uF 陶瓷 | E22 `VCC` | E22 `GND` | 尽量靠近 E22 |
| 220uF 或 470uF 电解 | ESP32 `5V` | ESP32 `GND` | 如果 USB 线较长，靠近开发板 5V 输入 |
| 100uF 电解，可选 | ESP32 `3V3` | ESP32 `GND` | 靠近开发板 3V3 |
| 0.1uF 陶瓷 | ESP32 `3V3` | ESP32 `GND` | 靠近开发板 3V3 |

## 6. 通信协议

第一版使用文本 CSV + CRC16，每行以 `\n` 结尾。

示例帧：

```text
HELLO,deviceId,currentClientId,battMv,crc16
HEARTBEAT,deviceId,currentClientId,battMv,msgId,crc16
SUBMIT,deviceId,clientId,roundId,msgId,red,blue,battMv,crc16
ACK,deviceId,clientId,roundId,msgId,OK,crc16
ACK,deviceId,clientId,roundId,msgId,OK_DUPLICATE,crc16
ACK,deviceId,clientId,roundId,msgId,ERR_ALREADY_SUBMITTED,crc16
ACK,deviceId,clientId,roundId,msgId,ERR_BAD_ROUND,crc16
STATUS,deviceId,clientId,roundId,roundOpen,submitted,crc16
STATUS,ALL,roundId,OPEN,crc16
STATUS,ALL,roundId,CLOSED,crc16
ASSIGN,deviceId,clientId,crc16
ASSIGN_ACK,deviceId,clientId,crc16
UNBIND,deviceId,crc16
UNBIND_ACK,deviceId,crc16
```

协议规则：

- CRC16 覆盖 `crc16` 前面的内容。
- 字段数量错误、CRC 错误、数字非法，直接丢弃。
- 分数限制为 `0..99`。
- 裁判机固件不再写死 `client1/client2/client3`，而是用 ESP32 MAC 地址生成唯一 `deviceId`。
- 服务端只接受已绑定的设备提交分数。
- 服务端维护 `deviceId -> clientId` 绑定关系，`clientId` 只能是 `client1`、`client2`、`client3`。
- `SUBMIT` 中的 `deviceId` 和 `clientId` 必须与服务端绑定关系一致。
- 服务端按 `deviceId + roundId + msgId` 去重。
- 重复包只重复 ACK，不重复计分。
- 第一版不做 `matchId`、shared token 或防串场机制，只保留 CRC。

## 7. 裁判机 ID 绑定方案

裁判机 ID 不通过修改代码或不同固件烧录设置。所有裁判机烧录同一份固件。

推荐方案：

```text
每台裁判机读取 ESP32 MAC
-> 生成唯一 deviceId，例如 A1B2C3D4
-> 服务端网页显示未绑定设备短码，例如 C3D4
-> 操作员在网页上绑定为 client1/client2/client3
-> 服务端下发 ASSIGN
-> 裁判机保存 clientId 到 NVS
-> 服务端保存 deviceId -> clientId 到 NVS
```

首次绑定流程：

```text
裁判机首次开机，没有 clientId
-> 显示 deviceId 后四位，例如 C3D4
-> 发送 HELLO,A1B2C3D4,UNASSIGNED,battMv,crc16
-> 服务端控制页显示未绑定设备 C3D4
-> 操作员点击“绑定为裁判1/2/3”
-> 服务端广播 ASSIGN,A1B2C3D4,client1,crc16
-> 目标裁判机保存 client1 到本机 NVS
-> 裁判机回复 ASSIGN_ACK,A1B2C3D4,client1,crc16
-> 裁判机显示 J1 或进入 00.00
```

重启恢复：

```text
裁判机重启
-> 从本机 NVS 读取 clientId
-> 发送 HELLO,deviceId,clientId,battMv,crc16
-> 服务端校验绑定关系
-> 返回 STATUS,deviceId,clientId,roundId,roundOpen,submitted,crc16
```

解绑方式：

- 服务端控制页可以解绑某个 `deviceId`。
- 裁判机建议提供本地清除绑定组合键，例如长按“提交 + 红 +1”5 秒。

解绑流程：

```text
服务端发送 UNBIND,deviceId,crc16
-> 目标裁判机清除本机 NVS 中的 clientId
-> 裁判机回复 UNBIND_ACK,deviceId,crc16
-> 裁判机回到未绑定状态，显示 deviceId 后四位
```

## 8. LoRa 信道访问与可靠性

信道访问策略：

```text
首次提交：随机等待 0~300ms 后发送
未收到 ACK：随机等待 300~1200ms 后重发
最多重发：5 次
```

服务端行为：

- 收到合法 `SUBMIT` 后返回 ACK。
- 如果是重复 `msgId`，返回 `OK_DUPLICATE`。
- 如果当前轮该裁判已经提交过，返回 `ERR_ALREADY_SUBMITTED`。
- 如果 `deviceId` 未绑定，返回错误或忽略提交，并在日志中记录。
- 如果 `deviceId` 与 `clientId` 不匹配，拒绝提交并记录日志。
- 对裁判机来说，`OK`、`OK_DUPLICATE`、`ERR_ALREADY_SUBMITTED` 都表示本轮已经被服务端确认，应进入锁定状态。

## 9. 裁判机状态机

```text
BOOT:
  初始化 LoRa UART、TM1637、按键、电池 ADC、低电量 LED
  读取 MAC 生成 deviceId
  从 NVS 读取 clientId
  发送 HELLO
  等待服务端 STATUS 或 ASSIGN

UNASSIGNED:
  未绑定裁判 ID
  显示 deviceId 后四位，例如 C3D4
  定期发送 HELLO
  等待服务端 ASSIGN

EDITING:
  显示当前比分，例如 03.05
  可加分、减分、清零
  短按提交键进入 SENDING

SENDING:
  显示 SEND
  发送 SUBMIT
  等待 ACK
  未收到 ACK 自动重发，最多 5 次
  收到确认后进入 LOCKED

LOCKED:
  显示 ----
  继续发送心跳
  等待 STATUS OPEN 解锁进入下一轮

ERROR:
  显示 Err
  保留当前分数
  允许再次提交或长按清零
```

显示码建议：

```text
CxDx    未绑定时显示 deviceId 后四位，具体按 7 段码能力简化
J1/J2/J3 绑定成功提示
03.05   当前比分
SEND    正在发送/等待确认
----    已提交/等待下一轮
Err     提交失败
```

心跳策略：

```text
未提交时：每 10 秒发送一次 HEARTBEAT
已提交/锁定后：每 15 秒发送一次 HEARTBEAT
```

低电量策略（第一版）：

```text
电池电压 < 3.7V：闪烁低电量 LED（GPIO2）提示，仍允许使用和提交
电池电压 >= 3.7V：LED 熄灭
```

第一版只做"提示不拦截"：任何电量都允许提交，避免比赛中掉电拦住裁判。
阈值 `BATTERY_LOW_MV` 与校准系数 `BATTERY_CALIBRATION` 在裁判机固件里，按电池、稳压模块和 ADC 分压实测校准。

## 10. 服务端逻辑

服务端职责：

- LoRa UART 按行读取。
- 校验 CSV + CRC16。
- 管理 `deviceId -> clientId` 绑定关系。
- 管理裁判状态、分数、轮次、总比分。
- 通过 WiFi AP 提供网页。
- 通过 NVS 保存比赛状态。
- 维护最近 50 条内存日志。

服务端绑定规则：

```text
client1/client2/client3 每个位置最多绑定一个 deviceId
同一个 deviceId 只能绑定一个 clientId
绑定变化必须保存到 NVS
未绑定设备只允许 HELLO/HEARTBEAT/ASSIGN_ACK/UNBIND_ACK，不允许 SUBMIT
```

轮次完成逻辑：

```text
所有已绑定裁判全部提交（正式比赛通常为 3 名；1 台联调时也可累计）
-> 计算本轮最终分
-> 累加总比分
-> roundOpen=false
-> 保存 NVS
-> 广播 STATUS,ALL,roundId,CLOSED 5 次
```

下一轮逻辑：

```text
普通下一轮：
  仅在 roundOpen=false 时允许
  roundId++
  清空本轮提交状态和分数
  roundOpen=true
  保存 NVS
  广播 STATUS,ALL,roundId,OPEN 5 次

强制下一轮：
  仅网页提供
  当前轮未完成时需要二次确认
  不累计未完成轮分数
  写入日志
```

广播策略：

- 服务端下一轮时广播 `STATUS,ALL,roundId,OPEN`。
- 服务端本轮完成时广播 `STATUS,ALL,roundId,CLOSED`。
- 每次广播 5 次，间隔 100~200ms。
- 裁判机按 `roundId` 幂等处理，重复包无副作用。

## 11. 计分规则

继续沿用现有规则，红蓝双方分别计算：

```text
正式 3 裁判时：3 个裁判中有 2 个或 3 个分数相同，采用相同分数
正式 3 裁判时：3 个都不同，采用最低分
1 台联调时：直接采用这台裁判提交的分数
2 台联调时：两台相同采用相同分数，不同则采用最低分
```

示例：

```text
5,5,7 -> 5
7,5,5 -> 5
5,7,5 -> 5
3,5,7 -> 3
0,2,4 -> 0
```

第一版业务上固定 3 名裁判，但代码按 `JUDGE_COUNT` 写循环，避免写死 `roundSubmitted[0] && roundSubmitted[1] && roundSubmitted[2]`。

## 12. 网页与控制

网页继续使用 HTTP 轮询。

当前 LoRa 服务端第一版使用 Arduino 内置 `WebServer` 直接渲染 HTML，暂不依赖 LittleFS 静态文件。网页实现放在服务端 `src/WebUi.cpp` / `src/WebUi.h`，`ServerWeb.cpp` 负责把服务端状态转换成网页状态快照并接入动作回调：

- 服务端启动 WiFi AP：SSID `ScoreServer`，密码 `score1234`。
- 默认 AP 地址为 `http://192.168.4.1/`。
- `/` 自动跳转到 `/score`。
- `/score` 为显示页，1 秒自动刷新，只显示两个队伍名称、总比分和倒计时。
- `/control` 为控制页，提供队伍名称修改、绑定、解绑、下一轮倒计时秒数输入、重置比分按钮；页面每 2 秒自动刷新一次裁判状态和最后通信时间，输入框获得焦点时暂停刷新，避免打断队名/倒计时输入。
- 绑定、解绑、下一轮、重置比分、修改队名使用 HTTP POST，并复用服务端已有串口命令/动作处理逻辑。
- 最近 50 条内存日志还未接入，后续在控制页补充。

- `/score` 轮询间隔从 500ms 改为 1000ms。
- 继续保留显示页 `/`。
- 继续保留控制页 `/control`。

控制页新增内容：

- 两个队伍名称，可在控制页修改并同步到显示页。
- 下一轮按钮输入倒计时秒数，确认后进入下一轮并启动倒计时。
- 每个裁判在线/离线状态，以及“最后通信”时间。这里显示的是距最近一次 HELLO/HEARTBEAT/SUBMIT 的时间，不是累计在线时长；收到新的心跳后该数值会变小。
- 每个裁判电池电压。
- 每个裁判当前轮是否已提交。
- 每个裁判当前轮分数。
- 裁判绑定状态：`client1/client2/client3` 分别绑定了哪个 `deviceId`。
- 未绑定设备列表。
- 绑定/解绑按钮。
- 最近 50 条内存日志。

绑定区域建议：

```text
裁判绑定
client1: C3D4 在线 3.82V [解绑]
client2: 未绑定
client3: A921 离线 35s [解绑]

未绑定设备
F09B 在线 3.79V [绑定为裁判1] [绑定为裁判2] [绑定为裁判3]
```

在线状态建议：

```text
<= 15s：在线
15~30s：弱连接
> 30s：离线
```

控制操作：

- 普通下一轮：仅本轮完成后允许。
- 强制下一轮：仅网页提供，必须二次确认；未完成轮不累计到总比分。
- 下一轮倒计时：控制页输入秒数，确认后 `roundId++`、清空本轮提交、广播新轮次状态，并启动显示页倒计时。
- 重置当前轮：清空本轮裁判提交，不改总比分。
- 重置总比分：网页二次确认；服务端实体按钮长按 3 秒。
- 绑定裁判：从未绑定设备列表选择目标裁判位。
- 解绑裁判：二次确认后清除服务端绑定，并向裁判机发送 `UNBIND`。

## 13. 持久化

服务端使用 ESP32 `Preferences` / NVS 保存比赛状态，不频繁写 LittleFS。

保存内容：

- 总比分。
- 队名。
- `roundId`。
- `roundOpen`。
- 本轮每个裁判是否已提交。
- 本轮每个裁判提交分数。
- `deviceId -> clientId` 绑定关系。
- 每个 `deviceId` 最近一次电池电压和在线时间，可只保存在 RAM，重启后重新通过 HELLO/HEARTBEAT 恢复。

保存时机：

- 裁判提交成功。
- 本轮完成并累计总分。
- 点击下一轮。
- 强制下一轮。
- 重置总分。
- 修改队名。

启动恢复：

```text
如果 NVS 中有有效状态：
  恢复总分、队名、roundId、本轮提交状态、设备绑定关系
否则：
  初始化新比赛
```

裁判机本机 NVS 保存内容：

```text
deviceId 不需要保存，每次由 MAC 生成
clientId 保存服务端下发的 client1/client2/client3
如果本地清除绑定，则删除 clientId
```

## 14. 项目结构

保留旧 WiFi 项目不动，新建 LoRa 版本：

```text
Projects/
  esp32-client/             旧 WiFi 裁判端，不动
  score_system/             旧 WiFi 服务端，不动

  score_lora/               父仓库（含设计文档；client/server/ScoreProtocol 以 git 子模块引用）
    lora_score_system_design.md
    score_client-lora/      裁判机固件（子模块）
      platformio.ini
      src/main.cpp          启动顺序和 loop 调度入口
      src/ClientState.h/.cpp
                            deviceId/clientId、NVS、轮次锁定、pending submit、本地分数等共享状态
      src/ClientLoraLink.h/.cpp
                            E22 UART 初始化、HELLO/HEARTBEAT/SUBMIT/ACK 相关发送、LoRa 收帧
      src/ClientProtocolHandlers.h/.cpp
                            STATUS/ACK/ASSIGN/UNBIND 入站业务处理
      src/ClientActions.h/.cpp
                            提交队列、重传、解锁、本地红蓝分数增减
      src/ClientButtons.h/.cpp
                            5 个录分按键扫描、去抖、短按/长按分发
      src/ClientDisplay.h/.cpp
                            TM1637 显示状态机和短暂覆盖显示
      src/BatteryMonitor.h/.cpp
                            GPIO15 电池 ADC 采样和低电量 LED
      src/ClientConsole.h/.cpp
                            submit/show 串口调试命令
    score_server-lora/      服务端固件（子模块）
      platformio.ini
      src/main.cpp          启动顺序和 loop 调度入口
      src/ServerState.h/.cpp
                            绑定表、未绑定设备、轮次、本轮提交、裁判在线/电量状态
      src/LoraLink.h/.cpp   E22 UART 初始化、LoRa 收发、协议发送帧组装
      src/ProtocolHandlers.h/.cpp
                            HELLO/HEARTBEAT/SUBMIT/ACK 业务处理
      src/ServerActions.h/.cpp
                            绑定、解绑、下一轮、重置、list 等复用动作
      src/SerialConsole.h/.cpp
                            串口命令解析
      src/ServerButtons.h/.cpp
                            GPIO5/GPIO12 实体按钮扫描
      src/StatusLeds.h/.cpp 4 个服务端状态灯
      src/ServerWeb.h/.cpp  WebUi 状态适配和动作回调
      src/WebUi.h           网页状态 DTO、动作回调接口
      src/WebUi.cpp         WiFi AP、WebServer 路由和 HTML 渲染
      （当前暂不需要 data/ 静态文件）
    shared/
      ScoreProtocol/        公共协议库（子模块）
        src/ScoreProtocol.h
        src/ScoreProtocol.cpp
        src/Crc16.h
        src/Crc16.cpp
      TM1637Display/        共享数码管驱动（普通目录，随父仓库提交，仅裁判机使用）
        src/TM1637Display.h
        src/TM1637Display.cpp
```

公共协议库职责：

- CRC16 计算。
- CSV 字段解析。
- 消息类型枚举。
- 组包。
- 字段数量校验。
- 数字范围校验。

业务状态机仍放在各自项目中。

裁判端不再使用多个编译环境区分裁判 ID。所有裁判机使用同一份固件：

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi
board_build.partitions = default_16MB.csv
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=0
    -DARDUINO_USB_CDC_ON_BOOT=0
lib_extra_dirs =
    ../shared
```

`lib_extra_dirs = ../shared` 让两端固件都能 include 公共协议库 `ScoreProtocol`；`TM1637Display` 只有裁判机会 include（服务端不接数码管）。服务端当前使用 Arduino `WebServer` 内嵌页面，页面代码集中在 `WebUi.cpp`；后续如果要恢复静态页面，再在服务端 `platformio.ini` 增加 LittleFS 配置。

裁判机启动时通过 MAC 生成 `deviceId`，通过服务端 `ASSIGN` 获取并保存 `clientId`。

## 15. 第一版实施顺序

1. 新建 `shared/ScoreProtocol`，实现 CRC16、CSV 组包与解析。
2. 新建 `score_server-lora`，先实现 LoRa 串口收包、ACK 回包和串口日志。
3. 新建 `score_client-lora`，实现 LoRa 发包、ACK 等待、随机退避和重发。
4. 裁判机读取 MAC 生成 `deviceId`，发送 `HELLO`。
5. 服务端显示未绑定设备，并能通过串口或网页下发 `ASSIGN`。
6. 裁判机保存 `clientId` 到 NVS，并回复 `ASSIGN_ACK`。
7. 跑通 1 台裁判机 + 1 台服务端的 `HELLO`、`ASSIGN`、`SUBMIT`、`ACK`。
8. 接入 TM1637，显示未绑定短码、`J1/J2/J3`、比分、`SEND`、`----`、`Err`。
9. 接入 5 个按键，实现短按加分、长按减分、长按提交清零。
10. 接入电池 ADC，实现电量上报和低电量 LED 提示。
11. 服务端接入现有网页逻辑，恢复 `/score`、`/control`。
12. 控制页增加裁判绑定、裁判状态、电量、最近 50 条日志。
13. 服务端加入 NVS 状态保存、设备绑定保存和启动恢复。
14. 加入 `STATUS OPEN/CLOSED` 广播。
15. 扩展到 3 台裁判机。
16. 测试三台同时提交、ACK 丢包、重复包、服务端重启、裁判机重启、裁判机解绑重绑。
17. 在真实室内 30~80 米、人群和墙体环境中测试。
18. 稳定后再考虑 PCB。

## 16. 第一版 BOM 参考

本 BOM 按“先做 1 台裁判机 + 1 台服务端”采购。后续扩展到 3 台裁判机时，把裁判机清单乘以 3 即可。

### 16.1 裁判机 BOM

| 物品 | 推荐购买型号/关键词 | 关键规格 | 常见尺寸 | 数量 | 避免买错 |
|---|---|---|---|---:|---|
| ESP32-S3 开发板 | `ESP32-S3-DevKitC-1 N16R8` 或 `ESP32-S3-WROOM-1-N16R8 开发板 USB-C` | 16MB Flash，8MB PSRAM，USB-C，2.54mm 排针 | 常见约 25mm x 55mm，不同厂家略有差异 | 1 | 不买 ESP32-C3/ESP32-S2；优先 N16R8，不买没有 PSRAM 的低配板 |
| LoRa UART 模块 | `亿佰特 E22-400T22D` | UART 透明传输，410.125~493.125MHz，22dBm，SMA 天线座，2.54mm 插针/半孔 | 约 21mm x 36mm | 1 | 第一版不买 `T22S` 贴片版；不要买 915MHz、868MHz、2.4GHz |
| LoRa 天线 | `470MHz SMA-J 胶棒天线`，也可搜 `433/470MHz SMA 公头天线` | 50Ω，SMA 公头带中针，直棒或弯头均可 | 常见长度 100~170mm | 1 | 确认是 SMA 公头，不是 IPEX；不要买 2.4GHz WiFi 天线 |
| 数码管模块 | `TM1637 四位数码管模块 0.56英寸` | 4 线接口：VCC/GND/DIO/CLK，红色或白色均可 | 常见约 42mm x 24mm | 1 | 要买带 TM1637 驱动板的模块，不买裸 4 位数码管 |
| 低电量 LED | `5mm LED` + `1K 1/4W 电阻` | 低电量指示，高电平点亮（GPIO2） | 5mm LED | 1 组 | LED 必须串限流电阻 |
| 录分按键 | `12x12x7.3mm 轻触开关 四脚` + 按键帽 | 瞬时按键，非自锁，适合手按 | 12mm x 12mm，本体高度 7.3/8/10mm 均可 | 5 | 不买自锁开关；6x6mm 太小，不推荐做手持录分键 |
| 18650 电池座 | `单节 18650 电池盒 带引线` 或 `单节 18650 电池座 2脚` | 单节，弹片式，能取出电池 | 常见约 20mm x 77mm x 18mm | 1 | 不买双节串联电池盒；注意有些带开关但开关质量一般 |
| 电源开关 | `拨动开关 2脚/3脚 额定1A以上` 或 `SS12D00G3 拨动开关` | 用于切断 18650 正极 | 常见约 13mm x 7mm x 6mm | 1 | 不买自复位按键当电源开关；额定电流不要太小 |
| 5V 升压模块 | `MT3608 升压模块 可调` 或 `SX1308 5V升压模块` | 输入 2~24V，输出调到 5.0V，建议峰值 2A | MT3608 常见约 36mm x 17mm | 1 | 上电前先用万用表调到 5V；不要买只降压的 MP1584/LM2596 |
| 可选 3.3V 稳压模块 | `AMS1117-3.3 电源模块` 或 `MP1584 降压模块 调到3.3V` | 从 5V 转 3.3V，给 E22/TM1637 供电，建议 >=500mA | AMS1117 小板常见约 25mm x 11mm | 0/1 | 如果用 MP1584，必须先调到 3.3V 再接模块；不要把 5V 直接接 ESP32 GPIO |
| 电池分压电阻 | `1/4W 1% 金属膜电阻 100K` | 100K 上拉 + 100K 下拉，电池满电 4.2V 时 ADC 约 2.1V | 直插电阻 | 各 1，建议多买 | 不建议用 10K/10K，待机耗电更大 |
| E22 旁大电容 | `470uF 10V 直插电解电容` | 低频储能，正极接 VCC，负极接 GND | 常见 8mm x 12mm 左右 | 1 | 电解电容有极性，不能接反；耐压选 10V 更稳 |
| 升压输出电容 | `220uF 10V 直插电解电容` 或 `470uF 10V 直插电解电容` | 并在 5V 升压模块 OUT+/OUT- | 常见 6.3~8mm 直径 | 1 | 不要省略，洞洞板长线供电时很有用 |
| ESP32 旁电容 | `100uF 10V 直插电解电容` | 并在 ESP32 5V/GND 或 3V3/GND 附近 | 常见 5mm x 11mm | 1 | 正负极不要接反 |
| 去耦电容 | `0.1uF 50V 陶瓷电容 104 直插` | 并在 E22、ESP32、电源模块供电脚旁 | 直插 2.54/5.08mm 脚距常见 | 4~6 | 洞洞板阶段买直插 104，不买 0603/0805 贴片 |
| 洞洞板 | `2.54mm 单面洞洞板 5x7cm` 或 `7x9cm` | 手工焊接原型 | 50mm x 70mm 或 70mm x 90mm | 1 | 先估算外壳和模块尺寸，宁可稍大 |
| 杜邦线/排针 | `2.54mm 杜邦线 母对母/公对母`、`2.54mm 排针` | 原型连线 | 常规 | 若干 | 距离测试前建议焊接固定，不长期依赖松散杜邦线 |
| 接线端子 | `KF301-2P 5.08mm 接线端子` 或 `XH2.54 2P座+线` | 电池、电源输入输出连接 | KF301 较大，XH2.54 更小 | 若干 | 电池线别只靠面包板插孔 |

### 16.2 服务端 BOM

| 物品 | 推荐购买型号/关键词 | 关键规格 | 常见尺寸 | 数量 | 避免买错 |
|---|---|---|---|---:|---|
| ESP32-S3 开发板 | `ESP32-S3-DevKitC-1 N16R8` 或 `ESP32-S3-WROOM-1-N16R8 开发板 USB-C` | 16MB Flash，8MB PSRAM，USB-C | 常见约 25mm x 55mm | 1 | 服务端更推荐 N16R8，给网页和后续功能留余量 |
| LoRa UART 模块 | `亿佰特 E22-400T22D` | UART，410.125~493.125MHz，22dBm，SMA 天线座 | 约 21mm x 36mm | 1 | 第一版先用 T22D；如果实测不够再换 T30D |
| 可选高功率 LoRa | `亿佰特 E22-400T30D` | UART，410.125~493.125MHz，30dBm，发射电流更高 | 比 T22D 更大，以商家页面为准 | 0/1 | T30D 对电源要求更高，不建议一开始给裁判机用 |
| LoRa 天线 | `470MHz SMA-J 胶棒天线` | 50Ω，SMA 公头带中针 | 常见 100~170mm | 1 | 不买 WiFi 2.4GHz 天线 |
| 下一轮按钮 | `12x12x7.3mm 轻触开关` 或 `16mm 金属按钮 自复位` | 瞬时按键，非自锁 | 12mm 或 16mm 开孔款 | 1 | 服务端按钮可以比裁判机大，便于操作 |
| 重置按钮 | `12x12x7.3mm 轻触开关` 或 `16mm 金属按钮 自复位` | 长按 3 秒触发重置 | 同上 | 1 | 不买自锁按钮 |
| USB 电源 | `5V 2A USB 电源适配器` | 输出 5V，>=2A，Type-C/Micro USB 看开发板接口 | 常规 | 1 | 不用电脑弱 USB 口做长期供电 |
| 可选 3.3V 稳压模块 | `AMS1117-3.3 电源模块` 或 `MP1584 降压模块 调到3.3V` | 给 E22 独立供电，建议 >=500mA | AMS1117 小板常见约 25mm x 11mm | 0/1 | 如果服务端 E22 用 ESP32 3V3 稳定，可不买 |
| E22 旁大电容 | `470uF 10V 直插电解电容` | 并在 E22 VCC/GND | 常见 8mm x 12mm | 1 | T30D 时可加到 2 个或换更大容量 |
| 电源输入电容 | `220uF 10V 直插电解电容` 或 `470uF 10V` | 并在 5V 输入/GND | 常见 6.3~8mm 直径 | 1 | 线长时建议保留 |
| 去耦电容 | `0.1uF 50V 陶瓷电容 104 直插` | 并在 E22、ESP32 供电脚旁 | 直插 | 2~4 | 洞洞板阶段买直插 |
| 状态灯 | `5mm LED` + `1K 1/4W 电阻` | 电源/工作/TX/RX 四个指示灯，高电平点亮 | 5mm LED | 4 组 | LED 必须串限流电阻；电源灯接 GPIO45 只能高电平点亮 |
| 洞洞板/端子/排线 | `2.54mm 洞洞板`、`KF301-2P`、`杜邦线` | 原型固定 | 常规 | 若干 | 服务端建议固定焊接，减少现场接触不良 |

### 16.3 采购数量建议

先做最小链路：

```text
裁判机 BOM x1
服务端 BOM x1
额外多买：
- 470MHz SMA 天线 x1
- E22-400T22D x1
- ESP32-S3 N16R8 x1
- 12x12 轻触开关若干
- 0.1uF/100uF/470uF 电容若干
```

原因是无线和电源调试时，备用模块能快速排除“模块损坏/焊接损坏/天线不匹配”的问题。

扩展到 3 台裁判机：

```text
裁判机 BOM x3
服务端 BOM x1
```

### 16.4 购买前检查清单

下单前逐项确认：

```text
ESP32：ESP32-S3，不是 ESP32-C3/S2；优先 N16R8
LoRa：E22-400T22D，410~493MHz，UART透明传输，不是 SPI，不是 915MHz
天线：470MHz 或 433/470MHz，SMA 公头带中针，不是 2.4GHz WiFi 天线
数码管：TM1637 驱动模块，不是裸数码管
按键：瞬时轻触，不是自锁
电源：18650 单节，5V 升压，不是降压模块
逻辑供电：E22/TM1637 优先 3.3V；如用独立稳压模块，输出必须先调好再接
电容：电解电容选 10V，陶瓷电容买直插 104
```

## 17. 第一版暂不做的内容

- 不做 PCB。
- 不内置 18650 充电。
- 不做撤销上一轮结果。
- 不做 LoRa 防串场 token 或加密签名。
- 不改旧 WiFi 项目。
- 不使用 WebSocket/SSE，网页第一版继续 HTTP 轮询。
- 不通过编译不同固件设置裁判 ID，裁判 ID 统一由服务端绑定下发。

## 18. 风险与后续优化

- 第一版不做防串场，如果附近有同频同参数设备，理论上可能串包。后续可加入 `matchId + sharedToken`。
- UART 透明 LoRa 模块底层可控性不如 SPI SX1262 模块。如果后续需要更复杂的信道管理、RSSI 统计、CAD 或低功耗优化，可迁移到 SPI LoRa 模块。
- T30D 发射功率更高，但对供电和发热要求更高。第一版优先 T22D，实测不够再升级。
- 洞洞板/杜邦线阶段接触不良概率高，距离测试前应尽量焊接固定。
- TM1637 四位数码管显示字母有限，`SEND`、`Err` 等需要自定义 7 段码。
