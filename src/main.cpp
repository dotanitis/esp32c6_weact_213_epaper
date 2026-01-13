#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

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

// ===== Weather data =====
struct WeatherData {
  float temp = NAN;
  float tempMin = NAN;
  float tempMax = NAN;
  int weatherId = -1;          // OpenWeather "id" code
  String main;                // "Clear", "Clouds", ...
  String description;         // "few clouds"
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

static void drawWeatherIcon(int x, int y, int weatherId, const String& main) {
  // OpenWeather condition groups:
  // 2xx thunderstorm, 3xx drizzle, 5xx rain, 6xx snow, 7xx atmosphere, 800 clear, 80x clouds
  if (weatherId >= 200 && weatherId < 300) { drawStorm(x, y); return; }
  if (weatherId >= 300 && weatherId < 600) { drawRain(x, y);  return; }
  if (weatherId >= 600 && weatherId < 700) { drawSnow(x, y);  return; }
  if (weatherId >= 700 && weatherId < 800) { drawMist(x, y);  return; }
  if (weatherId == 800) { drawSun(x + 36, y + 28); return; }
  if (weatherId > 800 && weatherId < 900) { drawCloud(x, y); return; }

  // fallback
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

  if (!isnan(w.temp))    tempNow = String(w.temp, 1);
  if (!isnan(w.tempMin)) tMin    = String(w.tempMin, 1);
  if (!isnan(w.tempMax)) tMax    = String(w.tempMax, 1);

  // Use UTF-8 degree symbol sometimes fails on some fonts;
  // fallback is "C" or use (char)247? We'll keep it safe: "C"
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

    // small divider line
    display.drawLine(0, 22, display.width(), 22, GxEPD_BLACK);

    // ---- Icon (right side) ----
    // tuned to sit nicely on the right
    drawWeatherIcon(display.width() - 66, 26, w.weatherId, w.main);

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
  prefs.end();
}

static void saveSettings(const String& apiKey, const String& city) {
  prefs.begin("weather", false);
  prefs.putString("apiKey", apiKey);
  prefs.putString("city", city);
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

  out.temp    = doc["main"]["temp"].as<float>();
  out.tempMin = doc["main"]["temp_min"].as<float>();
  out.tempMax = doc["main"]["temp_max"].as<float>();

  out.weatherId   = doc["weather"][0]["id"].as<int>();
  out.main        = String((const char*)doc["weather"][0]["main"]);
  out.description = String((const char*)doc["weather"][0]["description"]);

  Serial.printf("Weather: %.1f (min %.1f max %.1f) id=%d main=%s\n",
                out.temp, out.tempMin, out.tempMax, out.weatherId, out.main.c_str());

  return true;
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
    return;
  }

  WeatherData w;
  if (fetchWeather(w)) {
    renderWeather(w);
  } else {
    WeatherData err;
    err.main = "Weather ERR";
    renderWeather(err);
  }
}

void loop() {
  // You can later add periodic refresh here (e.g., every 30 minutes)
}
