// TM1637 四位数码管驱动实现。
// 负责 bit-bang 两线协议、数字/字符编码，以及裁判端比分和短文本显示。
#include "TM1637Display.h"
#include <Arduino.h>

// 7段数码管编码 (0~9, A~F)
static const uint8_t digitToSegment[] = {
    // 0     1     2     3     4     5     6     7     8     9
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    // A     B     C     D     E     F
    0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
};

TM1637Display::TM1637Display(uint8_t pinClk, uint8_t pinDIO) {
    m_pinClk = pinClk;
    m_pinDIO = pinDIO;
    m_brightness = 7; // 默认最大亮度

    // TM1637 两线总线空闲态为 CLK=HIGH、DIO=HIGH。
    pinMode(m_pinClk, OUTPUT);
    pinMode(m_pinDIO, OUTPUT);
    digitalWrite(m_pinClk, HIGH);
    digitalWrite(m_pinDIO, HIGH);
}

void TM1637Display::setBrightness(uint8_t brightness) {
    // TM1637 亮度只使用低 3 位，范围 0..7。
    m_brightness = brightness & 0x07;
}

void TM1637Display::setSegments(const uint8_t segments[], uint8_t length, uint8_t pos) {
    start();
    // 0x40：自动地址增加写数据模式，后续从起始地址连续写 length 位。
    writeByte(0x40); // 数据写入模式
    stop();

    start();
    // 0xC0 | pos：设置显示 RAM 起始地址，pos=0 是最左一位。
    writeByte(0xC0 | pos); // 设置起始地址

    for (uint8_t i = 0; i < length; i++) {
        // 每个字节 bit0..bit6 对应 a..g 段，bit7 对应小数点。
        writeByte(segments[i]);
    }

    stop();

    start();
    // 0x88：显示开；低 3 位叠加亮度。
    writeByte(0x88 | m_brightness); // 设置亮度
    stop();
}

void TM1637Display::clear() {
    uint8_t blank[] = {0, 0, 0, 0};
    setSegments(blank, 4);
}

uint8_t TM1637Display::encodeDigit(uint8_t digit) {
    return digitToSegment[digit & 0x0F];
}

uint8_t TM1637Display::encodeDigitWithDot(uint8_t digit) {
    return digitToSegment[digit & 0x0F] | 0x80;
}

void TM1637Display::start() {
    // start 条件：CLK 高电平期间 DIO 从高变低，然后拉低 CLK 开始传输。
    digitalWrite(m_pinDIO, LOW);
    delayMicros(2);
    digitalWrite(m_pinClk, LOW);
    delayMicros(2);
}

void TM1637Display::stop() {
    // stop 条件：CLK 高电平期间 DIO 从低变高。
    digitalWrite(m_pinDIO, LOW);
    delayMicros(2);
    digitalWrite(m_pinClk, HIGH);
    delayMicros(2);
    digitalWrite(m_pinDIO, HIGH);
    delayMicros(2);
}

bool TM1637Display::writeByte(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(m_pinClk, LOW);
        delayMicros(2);
        // TM1637 按低位优先发送，每个 bit 在 CLK 低电平时准备数据。
        digitalWrite(m_pinDIO, (b & 0x01) ? HIGH : LOW);
        delayMicros(2);
        // CLK 拉高后 TM1637 采样 DIO。
        digitalWrite(m_pinClk, HIGH);
        delayMicros(2);
        b >>= 1;
    }

    // 等待 ACK
    digitalWrite(m_pinClk, LOW);
    delayMicros(2);
    // 释放 DIO，改成输入，让 TM1637 有机会把线拉低作为 ACK。
    pinMode(m_pinDIO, INPUT);
    delayMicros(2);
    digitalWrite(m_pinClk, HIGH);
    delayMicros(2);
    // ACK 为低电平。当前业务不强制使用返回值，但保留给后续硬件排查。
    bool ack = (digitalRead(m_pinDIO) == LOW);
    pinMode(m_pinDIO, OUTPUT);
    delayMicros(2);
    digitalWrite(m_pinClk, LOW);
    delayMicros(2);

    return ack;
}

void TM1637Display::delayMicros(uint16_t micros) {
    delayMicroseconds(micros);
}

void TM1637Display::showNumberDec(int number, bool leading_zeros, uint8_t length, uint8_t pos) {
    uint8_t digits[4];

    if (leading_zeros) {
        // 显示前导零
        for (int i = 3; i >= 0; i--) {
            // 从最右位开始取余，保证十进制数字按正常顺序显示。
            digits[i] = encodeDigit(number % 10);
            number /= 10;
        }
    } else {
        // 不显示前导零
        int temp = number;
        int digitCount = 0;
        if (temp == 0) digitCount = 1;
        while (temp > 0) {
            // 计算实际位数，用来决定左侧留几个空白。
            digitCount++;
            temp /= 10;
        }

        temp = number;
        for (int i = 3; i >= 0; i--) {
            if (i < 4 - digitCount) {
                digits[i] = 0; // 空白
            } else {
                // 仍从右往左填入各位数字。
                digits[i] = encodeDigit(temp % 10);
                temp /= 10;
            }
        }
    }

    setSegments(digits, length, pos);
}

// 7 段码字模扩展：覆盖项目需要的字符。
// 大小写处理：字母不区分大小写，按可读性最好的"小写"形态展示（如 'r' / 'b' / 'd'），
// 个别字母（J/L/U/P）大小写形态接近，直接用。
// 未识别字符返回 0（全灭）。
uint8_t TM1637Display::encodeChar(char c) {
    if (c >= '0' && c <= '9') {
        // 数字直接复用 digitToSegment 表。
        return digitToSegment[c - '0'];
    }
    switch (c) {
        // 与 0..F 一致
        case 'A': case 'a': return 0x77;
        case 'B':           return 0x7C;  // 与 'b' 同形（大写 B 在 7 段里仍画成小写 b）
        case 'b':           return 0x7C;
        case 'C':           return 0x39;
        case 'c':           return 0x58;
        case 'D': case 'd': return 0x5E;
        case 'E': case 'e': return 0x79;
        case 'F': case 'f': return 0x71;
        // 项目用到的额外字符
        case 'H': case 'h': return 0x76;
        case 'I':           return 0x06;  // 与 1 同形
        case 'i':           return 0x10;  // 仅最右下竖
        case 'J': case 'j': return 0x1E;
        case 'L': case 'l': return 0x38;
        case 'N':           return 0x37;  // 简化，与 H 接近
        case 'n':           return 0x54;
        case 'O':           return 0x3F;  // 与 0 同形
        case 'o':           return 0x5C;
        case 'P': case 'p': return 0x73;
        case 'R':           return 0x77;  // 与 A 同形（7 段画不出真正大写 R）
        case 'r':           return 0x50;
        case 'S': case 's': return 0x6D;  // 与 5 同形
        case 'T': case 't': return 0x78;
        case 'U': case 'u': return 0x3E;
        case 'Y': case 'y': return 0x6E;
        case '-':           return 0x40;
        case ' ':           return 0x00;
        default:            return 0x00;
    }
}

void TM1637Display::showText(const char* text) {
    uint8_t segs[4] = {0, 0, 0, 0};
    if (text != nullptr) {
        for (uint8_t i = 0; i < 4 && text[i] != '\0'; i++) {
            // 最多显示四个字符，不足的位保持 0 即熄灭。
            segs[i] = encodeChar(text[i]);
        }
    }
    setSegments(segs, 4);
}

void TM1637Display::showScore(int red, int blue) {
    // 分数显示范围固定 0..99，防止调用方传入异常值导致显示错乱。
    if (red < 0) red = 0;
    if (red > 99) red = 99;
    if (blue < 0) blue = 0;
    if (blue > 99) blue = 99;

    uint8_t segs[4];
    // 格式固定为 RR.BB，小数点位于红方个位之后。
    segs[0] = encodeDigit((red / 10) % 10);
    segs[1] = encodeDigitWithDot(red % 10);   // 红方个位带小数点
    segs[2] = encodeDigit((blue / 10) % 10);
    segs[3] = encodeDigit(blue % 10);
    setSegments(segs, 4);
}

void TM1637Display::showHexTail(uint32_t value) {
    // 取低 16 位的 4 个 hex 半字节，从最高位到最低位排到 pos0..pos3。
    uint8_t segs[4];
    // 右移 12/8/4/0 位分别得到四个 hex 字符。
    segs[0] = encodeDigit((value >> 12) & 0x0F);
    segs[1] = encodeDigit((value >> 8) & 0x0F);
    segs[2] = encodeDigit((value >> 4) & 0x0F);
    segs[3] = encodeDigit(value & 0x0F);
    setSegments(segs, 4);
}
