# ESP32-C6 E-Paper Weather Display

A sleek, power-efficient weather display application for the **ESP32-C6** microcontroller with a **2.13-inch e-paper display**. This project fetches real-time weather data from the OpenWeather API and displays it beautifully on the e-paper screen with custom weather icons.

## Features

- 🌤️ **Real-time Weather Updates**: Fetches current weather conditions from OpenWeather API
- 📱 **WiFi Configuration Portal**: Easy WiFi setup via captive portal (AP name: "EPD-Setup")
- 💾 **NVS Persistent Storage**: Saves WiFi credentials and API settings locally
- 🎨 **Custom Weather Icons**: Vector-based B/W weather icons (sun, clouds, rain, snow, storm, mist)
- ⚡ **Power Efficient**: E-paper display consumes minimal power between updates
- 🌍 **Multi-location Support**: Display weather for any city worldwide
- 📊 **Detailed Temperature Info**: Shows current, min, and max temperatures
- 🎯 **ESP32-C6 Optimized**: Specifically designed for the WEACT ESP32-C6 DevKit

## Hardware Requirements

### Microcontroller
- **ESP32-C6 DevKit** (e.g., WEACT ESP32-C6-DevKitC-1)

### Display
- **2.13" E-Paper Display (B/W)**
  - Model: Supports GxEPD2 library compatibility
  - Resolution: 250×122 pixels
  - Communication: SPI

### Connections
- **Power**: 3.3V and GND
- **SPI Bus**: SCK, MOSI, MISO
- **Control Pins**: CS, DC, RST, BUSY (see pin configuration below)

### Additional Components
- USB cable for programming and power
- Micro SD card (optional, for future enhancements)

## Pin Configuration

| Function | GPIO Pin |
|----------|----------|
| SPI Clock (SCK) | GPIO 6 |
| SPI MOSI | GPIO 7 |
| Chip Select (CS) | GPIO 10 |
| Data/Command (DC) | GPIO 2 |
| Reset (RST) | GPIO 3 |
| Busy (BUSY) | GPIO 4 |

**Note**: These pins are optimized for the WEACT ESP32-C6 DevKit. Adjust in [src/main.cpp](src/main.cpp) if using different hardware.

## Software Dependencies

### Required Libraries
- **WiFiManager** (tzapu) - WiFi configuration and captive portal
- **ArduinoJson** (bblanchon) - JSON parsing for API responses
- **GxEPD2** - Graphics driver for e-paper displays
- **Arduino Core for ESP32** - Official Arduino support

### Font Assets (included)
- `FreeMonoBold9pt7b` - Small text (labels, API data)
- `FreeMonoBold12pt7b` - Medium text (weather condition)
- `FreeMonoBold18pt7b` - Large text (current temperature)

## Getting Started

### Prerequisites
- [PlatformIO](https://platformio.org/) installed (VS Code extension or CLI)
- OpenWeather API key (free tier available at [openweathermap.org](https://openweathermap.org/api))
- USB cable for ESP32-C6

### Installation

1. **Clone or Download** this repository:
   ```bash
   git clone https://github.com/dotanitis/esp32c6_weact_213_epaper.git
   cd esp32c6_weact_213_epaper
   ```

2. **Install Dependencies** (PlatformIO handles this automatically):
   - Open the project in VS Code with PlatformIO extension
   - Dependencies are defined in [platformio.ini](platformio.ini)

3. **Build and Flash**:
   ```bash
   pio run -t upload
   ```
   Or use the PlatformIO Build and Upload buttons in VS Code.

4. **Monitor Serial Output** (to debug):
   ```bash
   pio run -t monitor
   ```

## Configuration

### First Boot - WiFi Setup

1. **Power on** the ESP32-C6
2. **Connect to WiFi AP**: Look for network named **"EPD-Setup"**
3. **Open Browser**: Captive portal should appear (or go to `192.168.4.1`)
4. **Enter WiFi Credentials**:
   - WiFi SSID and password
   - OpenWeather API Key
   - City name (e.g., "Beer Sheva,IL" or "London,UK")
5. **Submit** and wait for connection

### Subsequent Boots
- Device automatically connects to saved WiFi
- Fetches weather data and displays it
- Settings are persisted in NVS (non-volatile storage)

### Manual Configuration
To reset stored settings, uncomment this line in `setup()`:
```cpp
// static const bool FORCE_CLEAR_SETTINGS = true;
```

Then upload and run once. This will clear all NVS data and force reconfiguration on next boot.

## OpenWeather API Setup

### Getting a Free API Key

1. Visit [openweathermap.org](https://openweathermap.org/api)
2. Sign up for a free account
3. Subscribe to the **Current Weather Data** API (free tier available)
4. Generate an API key from your account dashboard
5. Use this key during WiFi portal setup

### Supported Locations
Accepts city names in format:
- `"London"` or `"London,UK"`
- `"Beer Sheva,IL"`
- `"New York,US"`
- `"Tokyo,JP"`

See [OpenWeather City Names](https://openweathermap.org/find) for complete list.

## Display Information

### Screen Layout

The 2.13" e-paper display shows:

```
┌─────────────────────────────────┐
│ Today: Beer Sheva,IL   [WEATHER]│
├─────────────────────────────────┤
│                                 │
│           25.3°C               │
│                                 │
│            Clouds               │
│                                 │
│ Min: 18.2°C     Max: 28.5°C    │
└─────────────────────────────────┘
```

### Weather Icons
The display renders custom vector icons based on OpenWeather condition codes:

| Condition | Icon | Code Range |
|-----------|------|-----------|
| Clear/Sunny | ☀️ Sun | 800 |
| Clouds | ☁️ Cloud | 801-804 |
| Rain/Drizzle | 🌧️ Rain | 300-599 |
| Thunderstorm | ⚡ Storm | 200-299 |
| Snow | ❄️ Snow | 600-699 |
| Mist/Fog | 🌫️ Mist | 700-799 |

## Building the Project

### Build Only
```bash
pio run
```

### Build and Upload
```bash
pio run -t upload
```

### Full Clean Build
```bash
pio run -t clean
pio run -t upload
```

### Monitor Serial Output
```bash
pio run -t monitor
```

Output example:
```
ePaper Weather Display
---------------------
BOOT: starting...
WiFi: connected. IP=192.168.1.100
Weather: 25.3 (min 18.2 max 28.5) id=802 main=Clouds
```

## Usage

### Normal Operation

1. **Power On**: Device boots and displays weather
2. **Auto-refresh**: Currently displays once at startup
3. **WiFi Errors**: Shows "No WiFi" if connection fails
4. **API Errors**: Shows "Weather ERR" if weather fetch fails

### Troubleshooting

#### "No WiFi" Message
- WiFi portal timed out (3 minutes)
- Power cycle and reconnect to "EPD-Setup"
- Check WiFi credentials

#### "Weather ERR" Message
- API key not configured
- OpenWeather API is down
- Internet connection issue
- Check serial monitor for details

#### Serial Monitor Debugging
```bash
pio run -t monitor
```

Look for messages like:
- `HTTP GET code: 200` - Successful API call
- `JSON parse failed:` - Invalid API response
- `WiFi: failed to connect` - Connection issue

## Code Structure

```
src/main.cpp
├── Pin Configuration (lines 16-22)
├── Display Setup (lines 24-30)
├── Storage/Preferences (lines 35-40)
├── Weather Data Struct (lines 42-49)
├── Drawing Functions
│   ├── drawSun(), drawCloud(), drawRain(), etc.
│   └── drawWeatherIcon()
├── Display Rendering (renderWeather)
├── Settings Management
│   ├── loadSettings()
│   └── saveSettings()
├── WiFi Portal (ensureWiFiWithPortal)
├── Weather API (fetchWeather)
└── Setup/Loop (void setup(), void loop())
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `renderWeather()` | Renders weather data to e-paper display |
| `fetchWeather()` | Fetches data from OpenWeather API |
| `ensureWiFiWithPortal()` | Handles WiFi config and custom parameters |
| `loadSettings()` / `saveSettings()` | NVS storage management |
| `drawWeatherIcon()` | Renders appropriate icon for conditions |

## Future Enhancements

- [ ] **Periodic Refresh**: Add timer for updates every 30 minutes
- [ ] **Multiple Locations**: Display weather for multiple cities
- [ ] **Historical Data**: Show weather trends over time
- [ ] **Humidity & Pressure**: Display additional metrics
- [ ] **Timezone Support**: Show local time based on location
- [ ] **Deep Sleep Mode**: Maximize battery life on battery power
- [ ] **Forecast Display**: Show multi-day forecast
- [ ] **Custom Units**: Support imperial/metric toggle in UI
- [ ] **OTA Updates**: Firmware updates over WiFi

## Power Consumption

- **E-paper Display**: ~0mA (idle), ~5-10mA (refreshing)
- **WiFi**: ~80-160mA (active), ~0mA (off)
- **Microcontroller**: ~160-240mA (active), very low in sleep mode

The current code runs WiFi and weather fetch once at startup, then sits idle. Perfect for battery-powered operation with periodic wake-ups.

## Known Limitations

- Single weather location (can be enhanced to support multiple)
- No periodic auto-refresh (requires external timer or RTC)
- Basic error display (no detailed error codes)
- Limited to 2.13" display format

## Hardware Notes

### WEACT ESP32-C6 DevKit Specifics
- USB-C programming and power
- 2.4GHz WiFi (no 5GHz)
- Supports external antenna
- Integrated voltage regulator (5V → 3.3V)

### E-Paper Display Notes
- One-time programming capability (use carefully)
- Image retention possible with bright content
- Temperature range: -20°C to +60°C
- Typical lifespan: 100,000+ updates

## Troubleshooting Guide

### Device won't flash
```bash
# Check if board is detected
pio run -t monitor
```
- Verify USB cable connection
- Check Device Manager (Windows) or `lsusb` (Linux)
- Try different USB port

### WiFi won't connect
- Check AP "EPD-Setup" is visible
- Verify WiFi password is correct
- Check if 2.4GHz WiFi is available (C6 doesn't support 5GHz)

### Weather not displaying
- Check API key format (40 character hex string)
- Verify city name format (e.g., "City,Country")
- Check OpenWeather service status
- Review serial monitor for JSON parsing errors

### E-paper display corruption
- Restart device (power cycle)
- Try forcing display clear with `FORCE_CLEAR_SETTINGS`
- Check display ribbon cable connection

## Development

### Setting Up Development Environment

1. **VS Code** with **PlatformIO** extension
2. **Git** for version control
3. **Serial monitor** for debugging

### Building Locally

```bash
# Install dependencies
pio lib install

# Build
pio run

# Flash
pio run -t upload

# Monitor
pio run -t monitor
```

## API Reference

### OpenWeather Current Weather API

**Endpoint**: `https://api.openweathermap.org/data/2.5/weather`

**Parameters**:
- `q`: City name
- `appid`: API key
- `units`: `metric` (Celsius) or `imperial` (Fahrenheit)

**Response Fields Used**:
- `main.temp` - Current temperature
- `main.temp_min` - Minimum temperature
- `main.temp_max` - Maximum temperature
- `weather[0].id` - Weather condition code
- `weather[0].main` - Weather category name
- `weather[0].description` - Detailed description

## License

This project is open-source and available under the [MIT License](LICENSE).

## Contributing

Contributions are welcome! Feel free to:
- Report issues
- Suggest features
- Submit pull requests
- Improve documentation

## Credits

- **ESP32-C6 Support**: Espressif Systems
- **E-Paper Library**: Jean-Marc Zingg (GxEPD2)
- **WiFi Management**: tzapu (WiFiManager)
- **JSON Parsing**: Benoit Blanchon (ArduinoJson)
- **Weather Data**: OpenWeatherMap

## Contact & Support

- **GitHub**: [dotanitis/esp32c6_weact_213_epaper](https://github.com/dotanitis/esp32c6_weact_213_epaper)
- **Issues**: Report bugs and feature requests via GitHub Issues
- **Email**: dotanitis@gmail.com

## Changelog

### Version 1.0 (Initial Release)
- Basic weather display functionality
- WiFi configuration portal
- OpenWeather API integration
- Custom weather icons
- NVS settings persistence

---

**Last Updated**: January 13, 2026  
**Maintainer**: Dotan Hofling
