#ifndef TM1637DISPLAY_H
#define TM1637DISPLAY_H

#include <Arduino.h>

// TM1637 四位数码管 bit-bang 驱动。
// 协议要点：两线（CLK/DIO），上拉到 VCC；start/stop 信号由 DIO 在 CLK 高电平期间变化触发。
// 字模仅覆盖项目实际用到的字符（0-9、A-F、用于 SEND/Err/Lo.b/J/d/n/r/o/L/' '/'-' 等显示）。
class TM1637Display {
public:
    // 构造时立刻把 CLK/DIO 设为 OUTPUT 并拉高，模块进入空闲状态。
    // pinClk / pinDIO：连到 TM1637 模块对应丝印的 GPIO 编号。
    TM1637Display(uint8_t pinClk, uint8_t pinDIO);

    // 设置亮度（0..7）。仅更新本地状态，下一次 setSegments 才会随显示控制字下发。
    void setBrightness(uint8_t brightness);

    // 把一段 segment 字节写入显示。
    // segments[i] 的 bit0..bit6 = a..g 段、bit7 = 小数点，置 1 点亮。
    // length：要写入几位（≤4）。
    // pos：起始位置（0..3），默认 0 即从最左开始。
    void setSegments(const uint8_t segments[], uint8_t length, uint8_t pos = 0);

    // 把 4 位全部置 0x00，相当于熄屏（显示控制仍是 ON 状态）。
    void clear();

    // 把单个十六进制数字 0..15 编成 7 段码（不带小数点）。
    uint8_t encodeDigit(uint8_t digit);

    // 同上但点亮小数点。
    uint8_t encodeDigitWithDot(uint8_t digit);

    // 显示十进制整数。
    // leading_zeros：true 则补零到 length 位；false 则右对齐留空。
    // length：要占用几位（≤4）。
    // pos：起始位置。
    void showNumberDec(int number, bool leading_zeros = false, uint8_t length = 4, uint8_t pos = 0);

    // ===== 项目自定义便捷方法 =====

    // 把最长 4 字符的字符串显示出来；多余截断、不足右侧补空白。
    // 字符按内置 7 段码字模查表；不识别的字符按空白处理。
    // 不处理小数点；要点亮小数点请用 showScore 或直接构造 segments 调 setSegments。
    void showText(const char* text);

    // 显示"RR.BB"格式的分数。
    // red / blue 被钳到 0..99；小于 10 时左侧补 0（"03.05" 而非 " 3. 5"）。
    // 小数点点亮在第 2 位（红方个位之后），与设计文档第 9 节 "03.05" 描述一致。
    void showScore(int red, int blue);

    // 显示一个 32 位值的低 16 位 hex（4 个大写 hex 字符）。
    // 用作未绑定时的 deviceId 短码：例如 0xEA90A994 显示为 "A994"。
    void showHexTail(uint32_t value);

private:
    uint8_t m_pinClk;
    uint8_t m_pinDIO;
    uint8_t m_brightness;

    void start();
    void stop();
    bool writeByte(uint8_t b);
    void delayMicros(uint16_t micros);

    // 把单个可见 ASCII 字符编成 7 段码字节。
    // 支持：'0'..'9' / 'A'..'F'（大小写均可）/ 'J' / 'L' / 'N','n' / 'O','o' / 'R','r' / 'S','s' /
    //       'T','t' / 'U','u' / 'P','p' / 'H','h' / 'I','i' / 'd' / 'b' / 'y' / '-' / ' '。
    // 未识别字符返回 0（空白）。
    static uint8_t encodeChar(char c);
};

#endif // TM1637DISPLAY_H