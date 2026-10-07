#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// =====================================================
// WiFi Settings (نام و رمز وای‌فای خود را وارد کنید)
// =====================================================
const char* ssid     = "Amir";
const char* password = "amirshwww@09305415745";

// =====================================================
// OLED & TOUCH HARDWARE
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 8
#define OLED_SCL 9

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define TOUCH_PIN 4 // پین کلید/تاچ

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// TOUCH CONTROL (5-Screen System)
// =====================================================
int currentScreen = 0;           // 0: ساعت | 1: تقویم | 2: آب‌وهوا | 3: دلار | 4: طلا
bool lastButtonState = LOW;
bool currentButtonState = LOW;

// =====================================================
// NTP Settings (ساعت ایران)
// =====================================================
const long GMT_OFFSET_SEC = 3 * 3600 + 30 * 60; // GMT+3:30
const int DAYLIGHT_OFFSET_SEC = 0;
const char* ntpServer = "pool.ntp.org";

// =====================================================
// WEATHER SETTINGS
// =====================================================
const char* weatherUrl = "http://api.open-meteo.com/v1/forecast?latitude=35.6944&longitude=51.4215&current=temperature_2m,relative_humidity_2m,weather_code";

float currentTemp = 0.0;
int currentHumidity = 0;
int weatherCode = 0;
bool weatherDataLoaded = false;

unsigned long lastWeatherCheck = 0;
const unsigned long WEATHER_INTERVAL = 600000; // هر ۱۰ دقیقه

// =====================================================
// NAVASAN API SETTINGS (USD & GOLD)
// =====================================================
const char* navasanToken = "freeIFnMe5aIPY68oTy5aRgcVA2d8cLP";

long usdPriceToman = 0;
long goldPriceToman = 0;
bool usdDataLoaded = false;
bool goldDataLoaded = false;

unsigned long lastFinanceCheck = 0;
const unsigned long FINANCE_INTERVAL = 12 * 3600 * 1000UL; // هر ۱۲ ساعت (روزی ۲ بار)

// =====================================================
// NAMES IN FINGLISH
// =====================================================
const char* monthsFinglish[12] = {
  "Farvardin", "Ordibehesht", "Khordad",
  "Tir", "Mordad", "Shahrivar",
  "Mehr", "Aban", "Azar", "Dey", "Bahman", "Esfand"
};

const char* daysFinglish[7] = {
  "Yekshanbeh", "Doshanbeh", "Seshambe", 
  "Chaharshanbeh", "Panjshanbeh", "Jomeh", "Shanbeh"
};

// =====================================================
// HELPER FUNCTIONS
// =====================================================
const char* getWeatherCondition(int code) {
  if (code == 0) return "Clear";
  if (code >= 1 && code <= 3) return "Cloudy";
  if (code == 45 || code == 48) return "Foggy";
  if (code >= 51 && code <= 67) return "Rainy";
  if (code >= 71 && code <= 77) return "Snowy";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95) return "Thunderstorm";
  return "Unknown";
}

void drawCenteredText(const char* text, int y, int size) {
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.println(text);
}

void gregorianToJalali(int gy, int gm, int gd, int &jy, int &jm, int &jd) {
  int gDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int jDays[] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29};

  int gy2 = gy - 1600;
  int gm2 = gm - 1;
  int gd2 = gd - 1;

  long gDayNo = 365L * gy2 + (gy2 + 3) / 4 - (gy2 + 99) / 100 + (gy2 + 399) / 400;

  for (int i = 0; i < gm2; i++) {
    gDayNo += gDays[i];
  }

  if (gm2 > 1 && ((gy % 4 == 0 && gy % 100 != 0) || (gy % 400 == 0))) {
    gDayNo++;
  }

  gDayNo += gd2;

  long jDayNo = gDayNo - 79;
  long jNp = jDayNo / 12053;
  jDayNo %= 12053;

  jy = 979 + 33 * jNp + 4 * (jDayNo / 1461);
  jDayNo %= 1461;

  if (jDayNo >= 366) {
    jy += (jDayNo - 1) / 365;
    jDayNo = (jDayNo - 1) % 365;
  }

  int i;
  for (i = 0; i < 11 && jDayNo >= jDays[i]; i++) {
    jDayNo -= jDays[i];
  }

  jm = i + 1;
  jd = jDayNo + 1;
}

// =====================================================
// FETCH DATA FUNCTIONS
// =====================================================
void updateWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherUrl);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      if (!deserializeJson(doc, payload)) {
        currentTemp = doc["current"]["temperature_2m"];
        currentHumidity = doc["current"]["relative_humidity_2m"];
        weatherCode = doc["current"]["weather_code"];
        weatherDataLoaded = true;
      }
    }
    http.end();
  }
}

void updateFinancialData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String fullUrl = "https://api.navasan.tech/latest/?api_key=" + String(navasanToken);
    
    Serial.println("Fetching USD & Gold from Navasan...");

    if (http.begin(client, fullUrl)) {
      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          // ۱. دریافت قیمت دلار
          if (doc.containsKey("usd_sell") && doc["usd_sell"].containsKey("value")) {
            long usdRial = doc["usd_sell"]["value"].as<long>();
            if (usdRial > 0) {
              usdPriceToman = usdRial / 10; // تبدیل ریال به تومان
              usdDataLoaded = true;
              Serial.print("USD Price (Toman): ");
              Serial.println(usdPriceToman);
            }
          }

          // ۲. دریافت قیمت طلای ۱۸ عیار با کلید 18ayar یا geram18
          if (doc.containsKey("18ayar") && doc["18ayar"].containsKey("value")) {
            long goldRial = doc["18ayar"]["value"].as<long>();
            if (goldRial > 0) {
              goldPriceToman = goldRial / 10; // تبدیل ریال به تومان
              goldDataLoaded = true;
              Serial.print("Gold 18k Price (Toman): ");
              Serial.println(goldPriceToman);
            }
          } else if (doc.containsKey("geram18") && doc["geram18"].containsKey("value")) {
            long goldRial = doc["geram18"]["value"].as<long>();
            if (goldRial > 0) {
              goldPriceToman = goldRial / 10;
              goldDataLoaded = true;
              Serial.print("Gold 18k Price (Toman): ");
              Serial.println(goldPriceToman);
            }
          }
        } else {
          Serial.print("JSON Parse Error: ");
          Serial.println(error.f_str());
        }
      } else {
        Serial.print("Navasan HTTP Error: ");
        Serial.println(httpCode);
      }
      http.end();
    }
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT); 

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found!");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  drawCenteredText("Connecting...", 25, 1);
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, ntpServer);

  updateWeatherData();
  updateFinancialData();

  lastWeatherCheck = millis();
  lastFinanceCheck = millis();

  lastButtonState = digitalRead(TOUCH_PIN);
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  if (millis() - lastWeatherCheck >= WEATHER_INTERVAL) {
    lastWeatherCheck = millis();
    updateWeatherData();
  }

  // آپدیت روزی ۲ بار (هر ۱۲ ساعت)
  if (millis() - lastFinanceCheck >= FINANCE_INTERVAL) {
    lastFinanceCheck = millis();
    updateFinancialData();
  }

  currentButtonState = digitalRead(TOUCH_PIN);

  if (currentButtonState == HIGH && lastButtonState == LOW) {
    delay(50);
    if (digitalRead(TOUCH_PIN) == HIGH) {
      currentScreen = (currentScreen + 1) % 5;
    }
  }
  lastButtonState = currentButtonState;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    display.clearDisplay();
    drawCenteredText("Getting Time...", 28, 1);
    display.display();
    return;
  }

  int gy = timeinfo.tm_year + 1900;
  int gm = timeinfo.tm_mon + 1;
  int gd = timeinfo.tm_mday;
  int wday = timeinfo.tm_wday;

  int jy, jm, jd;
  gregorianToJalali(gy, gm, gd, jy, jm, jd);

  int hour   = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;

  display.clearDisplay();

  if (currentScreen == 0) {
    // صفحه ۱: ساعت
    char clockText[6];
    sprintf(clockText, "%02d:%02d", hour, minute);
    drawCenteredText(clockText, 18, 4);

  } else if (currentScreen == 1) {
    // صفحه ۲: تقویم
    drawCenteredText(daysFinglish[wday], 2, 1);
    display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

    char dayBuffer[4];
    sprintf(dayBuffer, "%02d", jd);
    drawCenteredText(dayBuffer, 18, 3);

    drawCenteredText(monthsFinglish[jm - 1], 44, 1);

    display.drawLine(0, 54, 128, 54, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 56);
    display.print(jy);

    char timeSmall[6];
    sprintf(timeSmall, "%02d:%02d", hour, minute);
    display.setCursor(96, 56);
    display.print(timeSmall);

  } else if (currentScreen == 2) {
    // صفحه ۳: آب و هوا
    drawCenteredText("TEHRAN WEATHER", 2, 1);
    display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

    if (weatherDataLoaded) {
      char tempStr[10];
      sprintf(tempStr, "%.1f C", currentTemp);
      drawCenteredText(tempStr, 18, 3);

      drawCenteredText(getWeatherCondition(weatherCode), 44, 1);

      display.drawLine(0, 54, 128, 54, SSD1306_WHITE);

      display.setTextSize(1);
      display.setCursor(4, 56);
      display.print("Hum: ");
      display.print(currentHumidity);
      display.print("%");

      char timeSmall[6];
      sprintf(timeSmall, "%02d:%02d", hour, minute);
      display.setCursor(96, 56);
      display.print(timeSmall);
    } else {
      drawCenteredText("Loading Weather...", 28, 1);
    }

  } else if (currentScreen == 3) {
    // صفحه ۴: قیمت دلار
    drawCenteredText("USD / TOMAN", 2, 1);
    display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

    if (usdDataLoaded) {
      char tomanStr[15];
      sprintf(tomanStr, "%ld", usdPriceToman);
      drawCenteredText(tomanStr, 20, 2);

      drawCenteredText("TOMAN", 40, 1);

      display.drawLine(0, 52, 128, 52, SSD1306_WHITE);

      char timeSmall[6];
      sprintf(timeSmall, "%02d:%02d", hour, minute);
      display.setTextSize(1);
      display.setCursor(4, 55);
      display.print("USD Rate");
      display.setCursor(96, 55);
      display.print(timeSmall);
    } else {
      drawCenteredText("Connection Error", 25, 1);
      drawCenteredText("Check Serial", 40, 1);
    }

  } else if (currentScreen == 4) {
    // صفحه ۵: قیمت طلا ۱۸ عیار
    drawCenteredText("GOLD 18K / TOMAN", 2, 1);
    display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

    if (goldDataLoaded) {
      char tomanStr[15];
      sprintf(tomanStr, "%ld", goldPriceToman);
      drawCenteredText(tomanStr, 20, 2);

      drawCenteredText("TOMAN / GRAM", 40, 1);

      display.drawLine(0, 52, 128, 52, SSD1306_WHITE);

      char timeSmall[6];
      sprintf(timeSmall, "%02d:%02d", hour, minute);
      display.setTextSize(1);
      display.setCursor(4, 55);
      display.print("Gold Rate");
      display.setCursor(96, 55);
      display.print(timeSmall);
    } else {
      drawCenteredText("Connection Error", 25, 1);
      drawCenteredText("Check Serial", 40, 1);
    }
  }

  display.display();
}