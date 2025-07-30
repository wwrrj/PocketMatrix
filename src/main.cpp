#include <FastLED.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <algorithm>
#include "animation.h"
#include "fonts.h"
#include "color.h"

// === LED 配置 ===
#define NUM_LEDS 256
#define DATA_PIN 2
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// === WiFi 配置 ===
const char *ssid = "1101";          // WiFi SSID
const char *password = "ycwyp0220"; // WiFi 密码

const uint16_t ANIMATION_FRAMES = 7;

// === 对比度增强参数 ===
const float CONTRAST_FACTOR = 1.5;
const uint8_t MIN_BRIGHTNESS = 30;

// === NTP 配置 ===
WiFiUDP ntpUDP;
// NTP服务器为ntp.aliyun.com，时区偏移为UTC+8 (北京时间)
NTPClient timeClient(ntpUDP, "ntp.aliyun.com", 8 * 3600);

// ===== 像素映射 =====
//       屏幕顺序
//     1         2
//     4         3
//
int getPixelIndex(int x, int y)
{
  int localX = x % 8;
  int localY = y % 8;
  int localOffset = localY * 8 + localX;

  if (y < 8)
  { // 第一行
    if (x < 8)
    {
      return localOffset; // 屏1 (左上) 0-63
    }
    else
    {
      return 64 + localOffset; // 屏2 (右上) 64-127
    }
  }
  else
  { // 第二行
    if (x < 8)
    {
      // 左下角为屏3 (192-255)
      return 192 + localOffset;
    }
    else
    {
      // 右下角为屏4 (128-191)
      return 128 + localOffset;
    }
  }
}

// ===== 绘制单个3x5数字 =====
void drawSmallDigit(int digit, int startX, int startY, CRGB color)
{
  if (digit < 0 || digit > 9)
    return;

  for (int y = 0; y < 5; y++)
  {
    byte rowData = FONT_3x5[digit][y];
    for (int x = 0; x < 3; x++)
    {
      if ((rowData >> (2 - x)) & 0x01)
      {
        int pixelX = startX + x;
        int pixelY = startY + y;
        if (pixelX >= 0 && pixelX < 16 && pixelY >= 0 && pixelY < 16)
        {
          leds[getPixelIndex(pixelX, pixelY)] = color;
        }
      }
    }
  }
}

// ===== 绘制冒号 =====
void drawSmallColon(int startX, int startY, CRGB color)
{
  for (int y = 0; y < 5; y++)
  {
    byte rowData = COLON_1x5[y];
    if ((rowData >> 4) & 0x01)
    {
      int pixelX = startX;
      int pixelY = startY + y;
      if (pixelX >= 0 && pixelX < 16 && pixelY >= 0 && pixelY < 16)
      {
        leds[getPixelIndex(pixelX, pixelY)] = color;
      }
    }
  }
}

// ===== 矩形绘制 =====
void drawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, CRGB color)
{
  if (x1 > x2)
    std::swap(x1, x2);
  if (y1 > y2)
    std::swap(y1, y2);

  // 填充矩形区域
  for (uint8_t y = y1; y <= y2; y++)
  {
    for (uint8_t x = x1; x <= x2; x++)
    {
      leds[getPixelIndex(x, y)] = color;
    }
  }
}

// ===== 初始化LED =====
void init_LEDs(int brightness)
{
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  FastLED.clear();
  FastLED.show();
}

// ===== 对比度增强 =====
uint8_t enhanceContrast(uint8_t value)
{
  float normalized = value / 255.0;
  float enhanced = pow(normalized, CONTRAST_FACTOR);
  uint8_t result = enhanced * 255;

  if (result < MIN_BRIGHTNESS && value > 0)
  {
    result = MIN_BRIGHTNESS;
  }
  return result;
}

// ===== 动画播放 =====
void playAnimation(int frameDelay = 80, int brightness = 100)
{
  uint8_t originalBrightness = FastLED.getBrightness();

  FastLED.setBrightness(brightness);

  for (uint16_t frame = 0; frame < ANIMATION_FRAMES; frame++)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi disconnected during animation!");
      break;
    }

    for (int y = 0; y < 16; y++)
    {
      for (int x = 0; x < 16; x++)
      {
        uint32_t color = pgm_read_dword(&(a[frame][y][x]));

        uint8_t g = (color >> 16) & 0xFF;
        uint8_t r = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        r = enhanceContrast(r);
        g = enhanceContrast(g);
        b = enhanceContrast(b);

        int index = getPixelIndex(x, y);
        leds[index] = CRGB(r, g, b);
      }
    }

    FastLED.show();
    delay(frameDelay);
  }

  FastLED.show();
}

// ===== 绘制日期函数 =====
void drawDate(int day)
{
  drawRectangle(0, 8, 8, 9, CRGB::Red);     // 清除日期区域
  drawRectangle(0, 10, 8, 15, CRGB::White); // 清除日期区域

  // 绘制日期数字
  if (day < 10)
    drawSmallDigit(day, 3, 10, CRGB::Black); // 单个数字
  else
    drawSmallDigit(day / 10, 1, 10, CRGB::Black), // 两位数
        drawSmallDigit(day % 10, 5, 10, CRGB::Black);

  FastLED.show();
}

// === 将32位十六进制RGB颜色转换为RGB分量 ===
CRGB hexToRGB(uint32_t hexColor)
{
  uint8_t r = (hexColor >> 16) & 0xFF; // 提取红色分量
  uint8_t g = (hexColor >> 8) & 0xFF;  // 提取绿色分量
  uint8_t b = hexColor & 0xFF;         // 提取蓝色分量

  return CRGB(r, g, b);
}

void draw_time_style1(int currentHour, int currentMinute, int currentSecond)
{
  // 清除LED
  FastLED.clear();

  // --- 绘制小时 ---
  drawSmallDigit(currentHour / 10, 1, 2, CRGB::White);
  drawSmallDigit(currentHour % 10, 5, 2, CRGB::White);

  // --- 绘制冒号 ---
  if (currentSecond % 2 == 0)
  {
    drawSmallColon(11, 2, CRGB::White);
  }

  // --- 绘制分钟 ---
  drawSmallDigit(currentMinute / 10, 8, 9, CRGB::White);
  drawSmallDigit(currentMinute % 10, 12, 9, CRGB::White);

  // --- 绘制分钟 ---
  drawSmallDigit(currentMinute / 10, 8, 9, CRGB::White);
  drawSmallDigit(currentMinute % 10, 12, 9, CRGB::White);
  FastLED.show();
  delay(1000);
}

void draw_time_style2(int currentHour, int currentMinute, int currentSecond)
{
  // 清除LED
  FastLED.clear();

  for (int i = 0; i < 16; i++)
  {
    leds[getPixelIndex(i,4)] = hexToRGB(color1[i]);
    leds[getPixelIndex(i,12)] = hexToRGB(color1[15 - i]);
  }

  // --- 绘制小时 ---
  drawSmallDigit(currentHour / 10, 0, 6, CRGB::White);
  drawSmallDigit(currentHour % 10, 4, 6, CRGB::White);

  // --- 绘制分钟 ---
  drawSmallDigit(currentMinute / 10, 9, 6, CRGB::White);
  drawSmallDigit(currentMinute % 10, 13, 6, CRGB::White);

  FastLED.show();
  delay(1000);
}

// TODO: ===== 整点动画触发 =====
bool animationPlayed = false;
int lastTriggeredHour = -1;

void setup()
{
  Serial.begin(115200);

  init_LEDs(50);

  Serial.print("Connecting to WiFi...");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  Serial.println("Attempting to get time from NTP server...");
  while (!timeClient.update())
  {
    Serial.println("Failed to get time, retrying...");
    delay(1000);
  }
  Serial.println("NTP client started and time synchronized.");
}

void loop()
{

  timeClient.update();

  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();
  int currentDay = timeClient.getDate();

  // playAnimation(100, 100); // 播放动画 TEST
  draw_time_style2(currentHour, currentMinute, currentSecond);
}