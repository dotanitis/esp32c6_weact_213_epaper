#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

#include <WiFiManager.h>     // tzapu
#include <ArduinoJson.h>     // bblanchon

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

//static const bool FORCE_CLEAR_SETTINGS = true;

// ===== Pins (your working wiring) =====
static const int PIN_SCK  = 6;
static const int PIN_MOSI = 7;
static const int PIN_CS   = 10;
static const int PIN_DC   = 2;
static const int PIN_RST  = 3;
static const int PIN_BUSY = 4;

// ===== Display: 2.13" B/W =====
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
  GxEPD2_213_BN(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY)
);

// ===== Storage =====
Preferences prefs;

// ===== Settings saved in NVS =====
static String g_apiKey;
static String g_cityQuery; // e.g. "Beer Sheva,IL"
static String g_units = "metric"; // "metric" or "imperial"
static uint32_t g_updateIntervalHours = 12; // Default 12 hours

// ===== Weather data =====
struct WeatherData {
  float temp = NAN;
  float tempMin = NAN;
  float tempMax = NAN;
  float feelsLike = NAN;       // "feels like" temperature
  int weatherId = -1;          // OpenWeather "id" code
  String main;                 // "Clear", "Clouds", ...
  String description;          // "few clouds"
  String iconCode;             // Icon code from API (e.g., "01d", "02n")
  unsigned long timestamp = 0; // Unix timestamp of data retrieval
};

static const char* OW_HOST = "api.openweathermap.org";

// ---------------- Vector icons (simple, B/W) ----------------
static void drawSun(int cx, int cy) {
  display.drawCircle(cx, cy, 12, GxEPD_BLACK);
  for (int a = 0; a < 360; a += 30) {
    float r = radians((float)a);
    int x1 = cx + (int)(cos(r) * 16);
    int y1 = cy + (int)(sin(r) * 16);
    int x2 = cx + (int)(cos(r) * 24);
    int y2 = cy + (int)(sin(r) * 24);
    display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
  }
}

static void drawCloud(int x, int y) {
  display.fillCircle(x + 16, y + 18, 12, GxEPD_BLACK);
  display.fillCircle(x + 34, y + 14, 16, GxEPD_BLACK);
  display.fillCircle(x + 52, y + 18, 12, GxEPD_BLACK);
  display.fillRect  (x + 16, y + 18, 36, 18, GxEPD_BLACK);
}

static void drawCenteredText(int y, const String& text, const GFXfont* font) {
  display.setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int x = (display.width() - w) / 2 - x1;
  display.setCursor(x, y);
  display.print(text);
}

static void drawRain(int x, int y) {
  drawCloud(x, y);
  for (int i = 0; i < 3; i++) {
    int rx = x + 22 + i * 14;
    display.drawLine(rx, y + 42, rx - 4, y + 54, GxEPD_BLACK);
  }
}

static String urlEncodeCity(String s) {
  s.replace(" ", "%20");
  return s;
}

static void drawStorm(int x, int y) {
  drawCloud(x, y);
  // lightning bolt
  display.fillTriangle(x + 34, y + 36, x + 24, y + 56, x + 38, y + 56, GxEPD_BLACK);
  display.fillTriangle(x + 38, y + 56, x + 30, y + 72, x + 48, y + 52, GxEPD_BLACK);
}

static void drawSnow(int x, int y) {
  drawCloud(x, y);
  for (int i = 0; i < 3; i++) {
    int sx = x + 22 + i * 14;
    int sy = y + 48;
    display.drawLine(sx - 4, sy, sx + 4, sy, GxEPD_BLACK);
    display.drawLine(sx, sy - 4, sx, sy + 4, GxEPD_BLACK);
    display.drawLine(sx - 3, sy - 3, sx + 3, sy + 3, GxEPD_BLACK);
    display.drawLine(sx - 3, sy + 3, sx + 3, sy - 3, GxEPD_BLACK);
  }
}

static void drawMist(int x, int y) {
  // simple fog lines
  display.drawLine(x, y + 18, x + 70, y + 18, GxEPD_BLACK);
  display.drawLine(x + 8, y + 30, x + 62, y + 30, GxEPD_BLACK);
  display.drawLine(x, y + 42, x + 70, y + 42, GxEPD_BLACK);
}

static void drawWeatherIcon(int x, int y, const String& iconCode, int weatherId, const String& main) {
  // Icon code mapping from OpenWeather API
  // Always use night icons per user preference (ignore day/night suffix)
  // Icon codes: 01=clear, 02=few clouds, 03=scattered, 04=broken, 09=shower rain,
  //             10=rain, 11=thunderstorm, 13=snow, 50=mist
  
  if (iconCode.length() >= 2) {
    String baseIcon = iconCode.substring(0, 2);
    
    if (baseIcon == "01") { drawSun(x + 36, y + 28); return; }        // Clear
    if (baseIcon == "02") { drawCloud(x, y); return; }                // Few clouds
    if (baseIcon == "03") { drawSnow(x, y); return; }                 // Scattered clouds -> Snow per user request
    if (baseIcon == "04") { drawCloud(x, y); return; }                // Broken clouds
    if (baseIcon == "09") { drawRain(x, y); return; }                 // Shower rain
    if (baseIcon == "10") { drawRain(x, y); return; }                 // Rain
    if (baseIcon == "11") { drawStorm(x, y); return; }                // Thunderstorm
    if (baseIcon == "13") { drawSnow(x, y); return; }                 // Snow
    if (baseIcon == "50") { drawMist(x, y); return; }                 // Mist
  }
  
  // Fallback to weather ID based drawing
  if (weatherId >= 200 && weatherId < 300) { drawStorm(x, y); return; }
  if (weatherId >= 300 && weatherId < 600) { drawRain(x, y);  return; }
  if (weatherId >= 600 && weatherId < 700) { drawSnow(x, y);  return; }
  if (weatherId >= 700 && weatherId < 800) { drawMist(x, y);  return; }
  if (weatherId == 800) { drawSun(x + 36, y + 28); return; }
  if (weatherId > 800 && weatherId < 900) { drawCloud(x, y); return; }
  
  // Final fallback
  if (main == "Clear") drawSun(x + 36, y + 28);
  else if (main == "Clouds") drawCloud(x, y);
  else if (main == "Rain" || main == "Drizzle") drawRain(x, y);
  else if (main == "Thunderstorm") drawStorm(x, y);
  else if (main == "Snow") drawSnow(x, y);
  else drawMist(x, y);
}

static void renderWeather(const WeatherData& w) {
  display.setRotation(1);
  display.setFullWindow();

  // Prepare strings (with decimals)
  String tempNow = "--.-";
  String tMin = "--.-";
  String tMax = "--.-";
  String feelsLike = "--.-";

  if (!isnan(w.temp))      tempNow = String(w.temp, 1);
  if (!isnan(w.tempMin))   tMin    = String(w.tempMin, 1);
  if (!isnan(w.tempMax))   tMax    = String(w.tempMax, 1);
  if (!isnan(w.feelsLike)) feelsLike = String(w.feelsLike, 1);

  // Format timestamp
  String timeStr = "--:--";
  String dateStr = "--/--";
  if (w.timestamp > 0) {
    time_t t = w.timestamp;
    struct tm* tm_info = localtime(&t);
    char timeBuf[16];
    char dateBuf[16];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", tm_info);
    strftime(dateBuf, sizeof(dateBuf), "%d/%m/%y", tm_info);
    timeStr = String(timeBuf);
    dateStr = String(dateBuf);
  }

  String bigLine = tempNow + "C";

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // ---- Header ----
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(8, 16);
    display.print("Today: ");
    display.print(g_cityQuery);

    // ---- Timestamp line (under header) ----
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(8, 32);
    display.print(dateStr);
    display.print(" ");
    display.print(timeStr);

    // small divider line
    display.drawLine(0, 38, display.width(), 38, GxEPD_BLACK);

    // ---- Icon (right side) ----
    drawWeatherIcon(display.width() - 66, 42, w.iconCode, w.weatherId, w.main);

    // ---- Big temperature (centered) ----
    // Keep it away from the icon area by centering but it’s fine visually on 2.13"
    display.setTextColor(GxEPD_BLACK);
    drawCenteredText(70, bigLine, &FreeMonoBold18pt7b);

    // ---- Condition line ----
    display.setFont(&FreeMonoBold12pt7b);
    // You can use w.description if you want (but it can be long)
    String cond = (w.main.length() ? w.main : String("Weather"));
    drawCenteredText(95, cond, &FreeMonoBold12pt7b);

    // ---- Min/Max row ----
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 118);
    display.print("Min: ");
    display.print(tMin);
    display.print("C");

    display.setCursor(130, 118);
    display.print("Max: ");
    display.print(tMax);
    display.print("C");

  } while (display.nextPage());

  display.hibernate();
}

// ---------------- Preferences helpers ----------------
static void loadSettings() {
  prefs.begin("weather", true);
  g_apiKey = prefs.getString("apiKey", "");     // no default secret
  g_cityQuery = prefs.getString("city", "Beer Sheva,IL");
  g_updateIntervalHours = prefs.getUInt("interval", 12); // Default 12 hours
  prefs.end();
}

static void saveSettings(const String& apiKey, const String& city) {
  prefs.begin("weather", false);
  prefs.putString("apiKey", apiKey);
  prefs.putString("city", city);
  prefs.putUInt("interval", g_updateIntervalHours);
  prefs.end();
}

// ---------------- WiFi portal + custom params ----------------
static bool ensureWiFiWithPortal() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // 3 minutes
  wm.setDebugOutput(true);

  // Custom fields shown in the portal
  WiFiManagerParameter p_apiKey("apikey", "OpenWeather API Key", g_apiKey.c_str(), 64);
  WiFiManagerParameter p_city("city", "City", g_cityQuery.c_str(), 64);

  wm.addParameter(&p_apiKey);
  wm.addParameter(&p_city);

  // If no saved WiFi or connect fails, it starts AP portal
  // AP name: "EPD-Setup"
  bool ok = wm.autoConnect("EPD-Setup");

  if (!ok) {
    Serial.println("WiFi: failed to connect or portal timed out.");
    return false;
  }

  // Save custom params (even if WiFi was already configured)
  String newApiKey = String(p_apiKey.getValue());
  String newCity   = String(p_city.getValue());

  newApiKey.trim();
  newCity.trim();
  if (newCity.length() == 0) newCity = "Beer Sheva,IL";

  // Only save if provided (API key must be non-empty for weather)
  if (newApiKey.length() > 0) {
    saveSettings(newApiKey, newCity);
    g_apiKey = newApiKey;
    g_cityQuery = newCity;
  } else {
    // Keep old values if user left blank
    Serial.println("WiFi portal: API key left empty, keeping stored key (if any).");
  }

  Serial.print("WiFi connected. IP=");
  Serial.println(WiFi.localIP());
  return true;
}

// ---------------- Weather fetch ----------------
static bool fetchWeather(WeatherData& out) {
  if (g_apiKey.length() == 0) {
    Serial.println("No OpenWeather API key stored. Open portal and set it.");
    return false;
  }

  // OpenWeather "current weather" endpoint:
  // https://api.openweathermap.org/data/2.5/weather?q=...&appid=...&units=metric

  String cityEncoded = urlEncodeCity(g_cityQuery);
  String url = String("https://") + OW_HOST + "/data/2.5/weather?q=" +
               cityEncoded + "&appid=" + g_apiKey + "&units=" + g_units;
  Serial.println(url);

  WiFiClientSecure client;
  client.setInsecure(); // simplest: skip cert validation

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  int code = https.GET();
  Serial.printf("HTTP GET code: %d\n", code);
  if (code != 200) {
    Serial.printf("HTTP GET failed, code=%d\n", code);
    String body = https.getString();
    Serial.println(body);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  // Parse JSON
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  out.temp      = doc["main"]["temp"].as<float>();
  out.tempMin   = doc["main"]["temp_min"].as<float>();
  out.tempMax   = doc["main"]["temp_max"].as<float>();
  out.feelsLike = doc["main"]["feels_like"].as<float>();

  out.weatherId   = doc["weather"][0]["id"].as<int>();
  out.main        = String((const char*)doc["weather"][0]["main"]);
  out.description = String((const char*)doc["weather"][0]["description"]);
  out.iconCode    = String((const char*)doc["weather"][0]["icon"]);
  
  // Store current time as timestamp
  out.timestamp = time(nullptr);

  Serial.printf("Weather: %.1f (feels %.1f, min %.1f max %.1f) id=%d main=%s icon=%s\n",
                out.temp, out.feelsLike, out.tempMin, out.tempMax, 
                out.weatherId, out.main.c_str(), out.iconCode.c_str());

  return true;
}

// ---------------- Time sync helper ----------------
static void syncTime() {
  // Configure NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time...");
  
  int retries = 0;
  while (time(nullptr) < 100000 && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();
  
  if (time(nullptr) > 100000) {
    Serial.println("Time synced successfully");
  } else {
    Serial.println("Time sync failed, using relative time");
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ePaper Weather Display");
  Serial.println("---------------------");
  Serial.println("BOOT: starting...");


  // ePaper electrical stability
  pinMode(PIN_BUSY, INPUT_PULLUP);

  

  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_BUSY, INPUT_PULLUP);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_RST, HIGH);

  // Force SPI pins (don’t rely on defaults)
  SPI.begin(PIN_SCK, -1 /*MISO*/, PIN_MOSI, PIN_CS);

  display.init(115200, true, 50, false);

  // if (FORCE_CLEAR_SETTINGS) {
  //   prefs.begin("weather", false);
  //   prefs.clear();
  //   prefs.end();
  //   Serial.println("Cleared NVS settings!");
  // }

  loadSettings();

  // Connect WiFi / portal if needed
  if (!ensureWiFiWithPortal()) {
    WeatherData dummy;
    dummy.main = "No WiFi";
    renderWeather(dummy);
    Serial.println("Going to sleep (WiFi failed)...");
    esp_sleep_enable_timer_wakeup(g_updateIntervalHours * 3600ULL * 1000000ULL);
    esp_deep_sleep_start();
  }

  // Sync time from NTP
  syncTime();

  WeatherData w;
  if (fetchWeather(w)) {
    renderWeather(w);
  } else {
    WeatherData err;
    err.main = "Weather ERR";
    renderWeather(err);
  }

  // Disconnect WiFi to save power
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Enter deep sleep for configured interval
  uint64_t sleepTime = g_updateIntervalHours * 3600ULL * 1000000ULL; // Convert hours to microseconds
  Serial.printf("Going to sleep for %u hours...\n", g_updateIntervalHours);
  esp_sleep_enable_timer_wakeup(sleepTime);
  esp_deep_sleep_start();
}

void loop() {
  // Not used - device wakes from deep sleep and runs setup() again
}
