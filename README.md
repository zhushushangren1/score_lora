# LoRa 裁判记分系统

本仓库是 LoRa 版裁判记分系统，包含服务端固件、裁判端固件、公共协议库和项目文档。

## 文档

- [USER_MANUAL.md](USER_MANUAL.md)：面向客户和现场工作人员的使用说明书，不要求了解代码。
- [lora_score_system_design.md](lora_score_system_design.md)：面向开发和维护人员的技术设计文档，包含接线、协议、状态机、BOM 和后续优化。

## 项目结构

```text
score_client-lora/      裁判端 ESP32-S3 固件
score_server-lora/      服务端 ESP32-S3 固件
shared/ScoreProtocol/   双端共用 LoRa 文本协议和 CRC16
shared/TM1637Display/   裁判端数码管驱动
```

## 当前硬件版本

- 服务端：ESP32-S3 + E22-400T22D + WiFi AP + Web 控制页/显示页 + 2 个实体按钮 + 4 个状态灯。
- 裁判端：ESP32-S3 + E22-400T22D + TM1637 四位数码管 + 5 个计分按钮 + 低电量灯 + 18650 供电。
- LoRa 接线两端一致：E22 `TXD -> GPIO41`，E22 `RXD -> GPIO40`，`AUX -> GPIO42`，`M0 -> GPIO38`，`M1 -> GPIO39`。

## 构建

服务端：

```powershell
cd score_server-lora
platformio run
```

裁判端：

```powershell
cd score_client-lora
platformio run
```

当前双端均保留 `lora-debug [on|off]` 串口诊断命令，默认关闭。现场通信异常时可临时打开，正常比赛时保持关闭。

