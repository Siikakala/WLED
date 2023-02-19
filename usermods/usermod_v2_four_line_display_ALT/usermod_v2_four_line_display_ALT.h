#pragma once

#include "wled.h"
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/
#include <U8g2lib.h>
#include "4LD_wled_fonts.c"

//
// Insired by the usermod_v2_four_line_display
//
// v2 usermod for using 128x32 or 128x64 i2c
// OLED displays to provide a four line display
// for WLED.
//
// Dependencies
// * This usermod REQURES the ModeSortUsermod
// * This Usermod works best, by far, when coupled
//   with RotaryEncoderUIUsermod.
//
// Make sure to enable NTP and set your time zone in WLED Config | Time.
//
// REQUIREMENT: You must add the following requirements to
// REQUIREMENT: "lib_deps" within platformio.ini / platformio_override.ini
// REQUIREMENT: *  U8g2  (the version already in platformio.ini is fine)
// REQUIREMENT: *  Wire
//

// The SCL and SDA pins are defined here.
#ifndef FLD_PIN_SCL
#define FLD_PIN_SCL i2c_scl
#endif
#ifndef FLD_PIN_SDA
#define FLD_PIN_SDA i2c_sda
#endif
#ifndef FLD_PIN_CLOCKSPI
#define FLD_PIN_CLOCKSPI spi_sclk
#endif
#ifndef FLD_PIN_DATASPI
#define FLD_PIN_DATASPI spi_mosi
#endif
#ifndef FLD_PIN_CS
#define FLD_PIN_CS spi_cs
#endif

#ifdef ARDUINO_ARCH_ESP32
#ifndef FLD_PIN_DC
#define FLD_PIN_DC 19
#endif
#ifndef FLD_PIN_RESET
#define FLD_PIN_RESET 26
#endif
#else
#ifndef FLD_PIN_DC
#define FLD_PIN_DC 12
#endif
#ifndef FLD_PIN_RESET
#define FLD_PIN_RESET 16
#endif
#endif

#ifndef FLD_TYPE
#ifndef FLD_SPI_DEFAULT
#define FLD_TYPE SSD1306
#else
#define FLD_TYPE SSD1306_SPI
#endif
#endif

// When to time out to the clock or blank the screen
// if SLEEP_MODE_ENABLED.
#define SCREEN_TIMEOUT_MS 60 * 1000 // 1 min

// Minimum time between redrawing screen in ms
#define REFRESH_RATE_MS 1000

// Extra char (+1) for null
#define LINE_BUFFER_SIZE 16 + 1
#define MAX_JSON_CHARS 19 + 1
#define MAX_MODE_LINE_SPACE 13 + 1

#ifdef ARDUINO_ARCH_ESP32
static TaskHandle_t Display_Task = nullptr;
void DisplayTaskCode(void *parameter);
#endif

typedef enum
{
  NONE = 0,
  SSD1306,      // U8X8_SSD1306_128X32_UNIVISION_HW_I2C
  SH1106,       // U8X8_SH1106_128X64_WINSTAR_HW_I2C
  SSD1306_64,   // U8X8_SSD1306_128X64_NONAME_HW_I2C
  SSD1305,      // U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C
  SSD1305_64,   // U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C
  SSD1306_SPI,  // U8X8_SSD1306_128X32_NONAME_HW_SPI
  SSD1306_SPI64 // U8X8_SSD1306_128X64_NONAME_HW_SPI
} DisplayType;

class FourLineDisplayUsermod : public Usermod
{
public:
  FourLineDisplayUsermod()
  {
    if (!instance)
      instance = this;
  }
  static FourLineDisplayUsermod *getInstance(void) { return instance; }

private:
  static FourLineDisplayUsermod *instance;
  bool initDone = false;
  volatile bool drawing = false;

  // HW interface & configuration
  // U8X8 *u8x8 = nullptr; // pointer to U8X8 display object
  U8G2 *u8g2 = nullptr;

#ifndef FLD_SPI_DEFAULT
  int8_t ioPin[5] = {FLD_PIN_SCL, FLD_PIN_SDA, -1, -1, -1}; // I2C pins: SCL, SDA
  uint32_t ioFrequency = 400000;                            // in Hz (minimum is 100000, baseline is 400000 and maximum should be 3400000)
#else
  int8_t ioPin[5] = {FLD_PIN_CLOCKSPI, FLD_PIN_DATASPI, FLD_PIN_CS, FLD_PIN_DC, FLD_PIN_RESET}; // SPI pins: CLK, MOSI, CS, DC, RST
  uint32_t ioFrequency = 1000000;                                                               // in Hz (minimum is 500kHz, baseline is 1MHz and maximum should be 20MHz)
#endif

  DisplayType type = FLD_TYPE;                // display type
  bool flip = false;                          // flip display 180°
  uint8_t contrast = 10;                      // screen contrast
  uint8_t lineHeight = 1;                     // 1 row or 2 rows
  uint16_t refreshRate = REFRESH_RATE_MS;     // in ms
  uint32_t screenTimeout = SCREEN_TIMEOUT_MS; // in ms
  bool sleepMode = true;                      // allow screen sleep?
  bool clockMode = false;                     // display clock
  bool showSeconds = true;                    // display clock with seconds
  bool enabled = true;
  bool contrastFix = false;

  // Next variables hold the previous known values to determine if redraw is
  // required.
  String knownSsid = apSSID;
  IPAddress knownIp = IPAddress(4, 3, 2, 1);
  uint8_t knownBrightness = 0;
  uint8_t knownEffectSpeed = 0;
  uint8_t knownEffectIntensity = 0;
  uint8_t knownMode = 0;
  uint8_t knownPalette = 0;
  uint8_t knownMinute = 99;
  uint8_t knownHour = 99;
  byte brightness100;
  byte fxspeed100;
  byte fxintensity100;
  bool knownnightlight = nightlightActive;
  bool wificonnected = interfacesInited;
  bool powerON = true;
  uint8_t *buf;

  bool displayTurnedOff = false;
  unsigned long nextUpdate = 0;
  unsigned long lastRedraw = 0;
  unsigned long overlayUntil = 0;

  // Set to 2 or 3 to mark lines 2 or 3. Other values ignored.
  byte markLineNum = 255;
  byte markColNum = 255;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _enabled[];
  static const char _contrast[];
  static const char _refreshRate[];
  static const char _screenTimeOut[];
  static const char _flip[];
  static const char _sleepMode[];
  static const char _clockMode[];
  static const char _showSeconds[];
  static const char _busClkFrequency[];
  static const char _contrastFix[];

  // If display does not work or looks corrupted check the
  // constructor reference:
  // https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
  // or check the gallery:
  // https://github.com/olikraus/u8g2/wiki/gallery

  // some displays need this to properly apply contrast
  /*
  void setVcomh(bool highContrast)
  {
    u8x8_t *u8x8_struct = u8x8->getU8x8();
    u8x8_cad_StartTransfer(u8x8_struct);
    u8x8_cad_SendCmd(u8x8_struct, 0x0db);                        // address of value
    u8x8_cad_SendArg(u8x8_struct, highContrast ? 0x000 : 0x040); // value 0 for fix, reboot resets default back to 64
    u8x8_cad_EndTransfer(u8x8_struct);
  }
  // */

  /**
   * Wrappers for screen drawing
   */
  void setFlipMode(uint8_t mode)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setFlipMode(mode);
  }
  void setContrast(uint8_t contrast)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setContrast(contrast);
  }
  void drawString(uint8_t col, uint8_t row, const char *string, bool ignoreLH = false)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setFont(u8g2_font_finderskeepers_tf);
    u8g2->drawUTF8(col, row, string);
    u8g2->sendBuffer();
  }
  void draw2x2String(uint8_t col, uint8_t row, const char *string)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setFont(u8g2_font_finderskeepers_tf);
    u8g2->drawStr(col, row, string);
    u8g2->sendBuffer();
  }
  void drawGlyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font, bool ignoreLH = false)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setFont(font);
    if (!ignoreLH && lineHeight == 2)
      u8g2->drawGlyph(col, row, glyph);
    else
      u8g2->drawGlyph(col, row, glyph);
    u8g2->sendBuffer();
  }
  void draw2x2Glyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setFont(font);
    u8g2->drawGlyph(col, row, glyph);
    u8g2->sendBuffer();
  }
  uint8_t getCols()
  {
    if (type == NONE || !enabled)
      return 0;
    return u8g2->getCols();
  }
  void clear()
  {
    if (type == NONE || !enabled)
      return;
    u8g2->clearBuffer();
  }
  void setPowerSave(uint8_t save)
  {
    if (type == NONE || !enabled)
      return;
    u8g2->setPowerSave(save);
  }
  void center(String &line, uint8_t width)
  {
    int len = line.length();
    if (len < width)
      for (byte i = (width - len) / 2; i > 0; i--)
        line = ' ' + line;
    for (byte i = line.length(); i < width; i++)
      line += ' ';
  }

public:
  // gets called once at boot. Do all initialization that doesn't depend on
  // network here
  void setup()
  {
    if (type == NONE || !enabled)
      return;

    PinOwner po = PinOwner::UM_FourLineDisplay;
    uint8_t hw_scl = i2c_scl < 0 ? HW_PIN_SCL : i2c_scl;
    uint8_t hw_sda = i2c_sda < 0 ? HW_PIN_SDA : i2c_sda;
    if (ioPin[0] < 0 || ioPin[1] < 0)
    {
      ioPin[0] = hw_scl;
      ioPin[1] = hw_sda;
    }
    po = PinOwner::HW_I2C; // allow multiple allocations of HW I2C bus pins
    PinManagerPinType pins[2] = {{ioPin[0], true}, {ioPin[1], true}};
    if (!pinManager.allocateMultiplePins(pins, 2, po))
    {
      type = NONE;
      return;
    }

    DEBUG_PRINTLN(F("Allocating display."));
    u8g2 = (U8G2 *)new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0); // No rotation, landscape

    lineHeight = 2;
    DEBUG_PRINTLN(F("Starting display."));
    u8g2->initDisplay();
    u8g2->clearDisplay();
    u8g2->setPowerSave(0);
    u8g2->setDrawColor(0);
    u8g2->setFontMode(1);
    setContrast(contrast); // Contrast setup will help to preserve OLED lifetime. In case OLED need to be brighter increase number up to 255
    //drawString(0, 0, "Loading...");
    overlayLogo(3500);
    onUpdateBegin(false); // create Display task
    initDone = true;
  }

  // gets called every time WiFi is (re-)connected. Initialize own network
  // interfaces here
  void connected()
  {
    knownSsid = WiFi.SSID();     // apActive ? apSSID : WiFi.SSID(); //apActive ? WiFi.softAPSSID() :
    knownIp = Network.localIP(); // apActive ? IPAddress(4, 3, 2, 1) : Network.localIP();
    networkOverlay(PSTR("NETWORK INFO"), 7000);
  }

  /**
   * Da loop.
   */
  void loop()
  {
#ifndef ARDUINO_ARCH_ESP32
    if (!enabled || strip.isUpdating())
      return;
    unsigned long now = millis();
    if (now < nextUpdate)
      return;
    nextUpdate = now + ((displayTurnedOff && clockMode && showSeconds) ? 1000 : refreshRate);
    redraw(false);
#endif
  }

  // function to update lastredraw
  void updateRedrawTime()
  {
    lastRedraw = millis();
  }

  /**
   * Redraw the screen (but only if things have changed
   * or if forceRedraw).
   */
  void redraw(bool forceRedraw)
  {
    bool needRedraw = false;
    unsigned long now = millis();

    if (type == NONE || !enabled)
      return;
    if (overlayUntil > 0)
    {
      if (now >= overlayUntil)
      {
        // Time to display the overlay has elapsed.
        overlayUntil = 0;
        forceRedraw = true;
      }
      else
      {
        // We are still displaying the overlay
        // Don't redraw.
        return;
      }
    }

    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing

    if (apActive && WLED_WIFI_CONFIGURED && now < 15000)
    {
      knownSsid = apSSID;
      networkOverlay(PSTR("NETWORK INFO"), 30000);
      return;
    }

    if (!needRedraw)
    {
      // Nothing to change.
      // Turn off display after 1 minutes with no change.
      if (sleepMode && !displayTurnedOff && (millis() - lastRedraw > screenTimeout))
      {
        clear();
        displaySleep(true);
      }
      return;
    }

    lastRedraw = now;

    // Turn the display back on
    wakeDisplay();

    drawAaris();
  }

  //*
  void drawAaris()
  {
    drawing = true;
    u8g2->drawXBMP(0, 0, 128, 64, aaris_logo_xbm);
    u8g2->sendBuffer();
    drawing = false;
  }
  // */

  /**
   * If there screen is off or in clock is displayed,
   * this will return true. This allows us to throw away
   * the first input from the rotary encoder but
   * to wake up the screen.
   */
  bool wakeDisplay()
  {
    if (type == NONE || !enabled)
      return false;
    if (displayTurnedOff)
    {
      unsigned long now = millis();
      while (drawing && millis() - now < 250)
        delay(1); // wait if someone else is drawing
      drawing = true;
      clear();
      // Turn the display back on
      displaySleep(false);
      // lastRedraw = millis();
      drawing = false;
      return true;
    }
    return false;
  }

  /**
   * Allows you to show one line and a glyph as overlay for a period of time.
   * Clears the screen and prints.
   * Used in Rotary Encoder usermod.
   */
  void overlay(const char *line1, long showHowLong, byte glyphType)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (glyphType > 0 && glyphType < 255)
    {
      if (lineHeight == 2)
        drawGlyph(5, 0, glyphType, u8x8_4LineDisplay_WLED_icons_6x6, true); // use 3x3 font with draw2x2Glyph() if flash runs short and comment out 6x6 font
      else
        drawGlyph(6, 0, glyphType, u8x8_4LineDisplay_WLED_icons_3x3, true);
    }
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, (glyphType < 255 ? 3 : 0) * lineHeight, buf.c_str());
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  /**
   * Allows you to show Akemi WLED logo overlay for a period of time.
   * Clears the screen and prints.
   */
  void overlayLogo(long showHowLong)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (lineHeight == 2)
    {
      // add a bit of randomness
      switch (millis() % 3)
      {
      case 0:
        // WLED
        draw2x2Glyph(0, 2, 1, u8x8_wled_logo_2x2);
        draw2x2Glyph(4, 2, 2, u8x8_wled_logo_2x2);
        draw2x2Glyph(8, 2, 3, u8x8_wled_logo_2x2);
        draw2x2Glyph(12, 2, 4, u8x8_wled_logo_2x2);
        break;
      case 1:
        // WLED Akemi
        drawGlyph(2, 2, 1, u8x8_wled_logo_akemi_4x4, true);
        drawGlyph(6, 2, 2, u8x8_wled_logo_akemi_4x4, true);
        drawGlyph(10, 2, 3, u8x8_wled_logo_akemi_4x4, true);
        break;
      case 2:
        // Akemi
        // draw2x2Glyph( 5, 0, 12, u8x8_4LineDisplay_WLED_icons_3x3); // use this if flash runs short and comment out 6x6 font
        drawGlyph(5, 0, 12, u8x8_4LineDisplay_WLED_icons_6x6, true);
        drawString(6, 6, "WLED");
        break;
      }
    }
    else
    {
      switch (millis() % 3)
      {
      case 0:
        // WLED
        draw2x2Glyph(0, 0, 1, u8x8_wled_logo_2x2);
        draw2x2Glyph(4, 0, 2, u8x8_wled_logo_2x2);
        draw2x2Glyph(8, 0, 3, u8x8_wled_logo_2x2);
        draw2x2Glyph(12, 0, 4, u8x8_wled_logo_2x2);
        break;
      case 1:
        // WLED Akemi
        drawGlyph(2, 0, 1, u8x8_wled_logo_akemi_4x4);
        drawGlyph(6, 0, 2, u8x8_wled_logo_akemi_4x4);
        drawGlyph(10, 0, 3, u8x8_wled_logo_akemi_4x4);
        break;
      case 2:
        // Akemi
        // drawGlyph( 6, 0, 12, u8x8_4LineDisplay_WLED_icons_4x4); // a bit nicer, but uses extra 1.5k flash
        draw2x2Glyph(6, 0, 12, u8x8_4LineDisplay_WLED_icons_2x2);
        break;
      }
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  /**
   * Allows you to show two lines as overlay for a period of time.
   * Clears the screen and prints.
   * Used in Auto Save usermod
   */
  void overlay(const char *line1, const char *line2, long showHowLong)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 1 * lineHeight, buf.c_str());
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 2 * lineHeight, buf.c_str());
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void overlay(const char *line1, const char *line2, const char *line3, long showHowLong)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 0, buf.c_str());
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 2 * lineHeight, buf.c_str());
    }
    if (line3)
    {
      String buf = line3;
      center(buf, getCols());
      drawString(0, 3 * lineHeight, buf.c_str());
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void overlay(const char *line1, const char *line2, const char *line3, const char *line4, long showHowLong, bool ignoreLH = false)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 0, buf.c_str(), ignoreLH);
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 1, buf.c_str(), ignoreLH);
    }
    if (line3)
    {
      String buf = line3;
      center(buf, getCols());
      drawString(0, 2, buf.c_str(), ignoreLH);
    }
    if (line4)
    {
      String buf = line4;
      center(buf, getCols());
      drawString(0, 3, buf.c_str(), ignoreLH);
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void overlay(const char *line1, const char *line2, const char *line3, const char *line4, const char *line5, long showHowLong, bool ignoreLH = false)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 0, buf.c_str(), ignoreLH);
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 1, buf.c_str(), ignoreLH);
    }
    if (line3)
    {
      String buf = line3;
      center(buf, getCols());
      drawString(0, 2, buf.c_str(), ignoreLH);
    }
    if (line4)
    {
      String buf = line4;
      center(buf, getCols());
      drawString(0, 3, buf.c_str(), ignoreLH);
    }
    if (line5)
    {
      String buf = line4;
      center(buf, getCols());
      drawString(0, 4, buf.c_str(), ignoreLH);
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void overlay(const char *line1, const char *line2, const char *line3, const char *line4, const char *line5, const char *line6, long showHowLong, bool ignoreLH = false)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 0, buf.c_str(), ignoreLH);
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 1, buf.c_str(), ignoreLH);
    }
    if (line3)
    {
      String buf = line3;
      center(buf, getCols());
      drawString(0, 2, buf.c_str(), ignoreLH);
    }
    if (line4)
    {
      String buf = line4;
      center(buf, getCols());
      drawString(0, 3, buf.c_str(), ignoreLH);
    }
    if (line5)
    {
      String buf = line5;
      center(buf, getCols());
      drawString(0, 4, buf.c_str(), ignoreLH);
    }
    if (line6)
    {
      String buf = line6;
      center(buf, getCols());
      drawString(0, 5, buf.c_str(), ignoreLH);
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void overlay(const char *line1, const char *line2, const char *line3, const char *line4, const char *line5, const char *line6, const char *line7, long showHowLong, bool ignoreLH = false)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 0, buf.c_str(), ignoreLH);
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 1, buf.c_str(), ignoreLH);
    }
    if (line3)
    {
      String buf = line3;
      center(buf, getCols());
      drawString(0, 2, buf.c_str(), ignoreLH);
    }
    if (line4)
    {
      String buf = line4;
      center(buf, getCols());
      drawString(0, 3, buf.c_str(), ignoreLH);
    }
    if (line5)
    {
      String buf = line5;
      center(buf, getCols());
      drawString(0, 4, buf.c_str(), ignoreLH);
    }
    if (line6)
    {
      String buf = line6;
      center(buf, getCols());
      drawString(0, 5, buf.c_str(), ignoreLH);
    }
    if (line7)
    {
      String buf = line7;
      center(buf, getCols());
      drawString(0, 6, buf.c_str(), ignoreLH);
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void overlay(const char *line1, const char *line2, const char *line3, const char *line4, const char *line5, const char *line6, const char *line7, const char *line8, long showHowLong, bool ignoreLH = false)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      String buf = line1;
      center(buf, getCols());
      drawString(0, 0, buf.c_str(), ignoreLH);
    }
    if (line2)
    {
      String buf = line2;
      center(buf, getCols());
      drawString(0, 1, buf.c_str(), ignoreLH);
    }
    if (line3)
    {
      String buf = line3;
      center(buf, getCols());
      drawString(0, 2, buf.c_str(), ignoreLH);
    }
    if (line4)
    {
      String buf = line4;
      center(buf, getCols());
      drawString(0, 3, buf.c_str(), ignoreLH);
    }
    if (line5)
    {
      String buf = line5;
      center(buf, getCols());
      drawString(0, 4, buf.c_str(), ignoreLH);
    }
    if (line6)
    {
      String buf = line6;
      center(buf, getCols());
      drawString(0, 5, buf.c_str(), ignoreLH);
    }
    if (line7)
    {
      String buf = line7;
      center(buf, getCols());
      drawString(0, 6, buf.c_str(), ignoreLH);
    }
    if (line8)
    {
      String buf = line8;
      center(buf, getCols());
      drawString(0, 7, buf.c_str(), ignoreLH);
    }
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  void networkOverlay(const char *line1, long showHowLong)
  {
    unsigned long now = millis();
    while (drawing && millis() - now < 250)
      delay(1); // wait if someone else is drawing
    drawing = true;

    String line;
    // Turn the display back on
    if (!wakeDisplay())
      clear();
    // Print the overlay
    if (line1)
    {
      line = line1;
      center(line, getCols());
      drawString(0, 0, line.c_str());
    }
    // Second row with Wifi name
    line = knownSsid.substring(0, getCols() > 1 ? getCols() - 2 : 0);
    if (line.length() < getCols())
      center(line, getCols());
    drawString(0, lineHeight, line.c_str());
    // Print `~` char to indicate that SSID is longer, than our display
    if (knownSsid.length() > getCols())
    {
      drawString(getCols() - 1, 0, "~");
    }
    // Third row with IP and Password in AP Mode
    line = knownIp.toString();
    center(line, getCols());
    drawString(0, lineHeight * 2, line.c_str());
    line = "";
    if (apActive)
    {
      line = apPass;
    }
    else if (strcmp(serverDescription, "WLED") != 0)
    {
      line = serverDescription;
    }
    center(line, getCols());
    drawString(0, lineHeight * 3, line.c_str());
    overlayUntil = millis() + showHowLong;
    drawing = false;
  }

  /**
   * Enable sleep (turn the display off) or clock mode.
   */
  void displaySleep(bool enabled)
  {
    if (enabled)
    {
      displayTurnedOff = true;
      setPowerSave(1);
    }
    else
    {
      displayTurnedOff = false;
      setPowerSave(0);
    }
  }

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif
  void onUpdateBegin(bool init)
  {
#ifdef ARDUINO_ARCH_ESP32
    if (init && Display_Task)
    {
      vTaskSuspend(Display_Task); // update is about to begin, disable task to prevent crash
    }
    else
    {
      // update has failed or create task requested
      if (Display_Task)
        vTaskResume(Display_Task);
      else
        xTaskCreatePinnedToCore(
            [](void *par) { // Function to implement the task
              // see https://www.freertos.org/vtaskdelayuntil.html
              const TickType_t xFrequency = REFRESH_RATE_MS * portTICK_PERIOD_MS / 2;
              TickType_t xLastWakeTime = xTaskGetTickCount();
              for (;;)
              {
                delay(1);                                    // DO NOT DELETE THIS LINE! It is needed to give the IDLE(0) task enough time and to keep the watchdog happy.
                                                             // taskYIELD(), yield(), vTaskDelay() and esp_task_wdt_feed() didn't seem to work.
                vTaskDelayUntil(&xLastWakeTime, xFrequency); // release CPU, by doing nothing for REFRESH_RATE_MS millis
                FourLineDisplayUsermod::getInstance()->redraw(false);
              }
            },
            "4LD",         // Name of the task
            3072,          // Stack size in words
            NULL,          // Task input parameter
            1,             // Priority of the task (not idle)
            &Display_Task, // Task handle
            ARDUINO_RUNNING_CORE);
    }
#endif
  }

  /*
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
   * Below it is shown how this could be used for e.g. a light sensor
   */
  // void addToJsonInfo(JsonObject& root) {
  // JsonObject user = root["u"];
  // if (user.isNull()) user = root.createNestedObject("u");
  // JsonArray data = user.createNestedArray(F("4LineDisplay"));
  // data.add(F("Loaded."));
  //}

  /*
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  // void addToJsonState(JsonObject& root) {
  // }

  /*
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  // void readFromJsonState(JsonObject& root) {
  //   if (!initDone) return;  // prevent crash on boot applyPreset()
  // }

  void appendConfigData()
  {
    oappend(SET_F("dd=addDropdown('4LineDisplay','type');"));
    oappend(SET_F("addOption(dd,'None',0);"));
    oappend(SET_F("addOption(dd,'SSD1306',1);"));
    oappend(SET_F("addOption(dd,'SH1106',2);"));
    oappend(SET_F("addOption(dd,'SSD1306 128x64',3);"));
    oappend(SET_F("addOption(dd,'SSD1305',4);"));
    oappend(SET_F("addOption(dd,'SSD1305 128x64',5);"));
    oappend(SET_F("addOption(dd,'SSD1306 SPI',6);"));
    oappend(SET_F("addOption(dd,'SSD1306 SPI 128x64',7);"));
    oappend(SET_F("addInfo('4LineDisplay:pin[]',0,'<i>-1 use global</i>','I2C/SPI CLK');"));
    oappend(SET_F("addInfo('4LineDisplay:pin[]',1,'<i>-1 use global</i>','I2C/SPI DTA');"));
    oappend(SET_F("addInfo('4LineDisplay:pin[]',2,'','SPI CS');"));
    oappend(SET_F("addInfo('4LineDisplay:pin[]',3,'','SPI DC');"));
    oappend(SET_F("addInfo('4LineDisplay:pin[]',4,'','SPI RST');"));
  }

  /*
   * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
   * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
   * If you want to force saving the current state, use serializeConfig() in your loop().
   *
   * CAUTION: serializeConfig() will initiate a filesystem write operation.
   * It might cause the LEDs to stutter and will cause flash wear if called too often.
   * Use it sparingly and always in the loop, never in network callbacks!
   *
   * addToConfig() will also not yet add your setting to one of the settings pages automatically.
   * To make that work you still have to add the setting to the HTML, xml.cpp and set.cpp manually.
   *
   * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
   */
  void addToConfig(JsonObject &root)
  {
    // determine if we are using global HW pins (data & clock)
    int8_t hw_dta, hw_clk;
    if ((type == SSD1306_SPI || type == SSD1306_SPI64))
    {
      hw_clk = spi_sclk < 0 ? HW_PIN_CLOCKSPI : spi_sclk;
      hw_dta = spi_mosi < 0 ? HW_PIN_DATASPI : spi_mosi;
    }
    else
    {
      hw_clk = i2c_scl < 0 ? HW_PIN_SCL : i2c_scl;
      hw_dta = i2c_sda < 0 ? HW_PIN_SDA : i2c_sda;
    }

    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;

    JsonArray io_pin = top.createNestedArray("pin");
    for (int i = 0; i < 5; i++)
    {
      if (i == 0 && ioPin[i] == hw_clk)
        io_pin.add(-1); // do not store global HW pin
      else if (i == 1 && ioPin[i] == hw_dta)
        io_pin.add(-1); // do not store global HW pin
      else
        io_pin.add(ioPin[i]);
    }
    top["type"] = type;
    top[FPSTR(_flip)] = (bool)flip;
    top[FPSTR(_contrast)] = contrast;
    top[FPSTR(_contrastFix)] = (bool)contrastFix;
#ifndef ARDUINO_ARCH_ESP32
    top[FPSTR(_refreshRate)] = refreshRate;
#endif
    top[FPSTR(_screenTimeOut)] = screenTimeout / 1000;
    top[FPSTR(_sleepMode)] = (bool)sleepMode;
    top[FPSTR(_clockMode)] = (bool)clockMode;
    top[FPSTR(_showSeconds)] = (bool)showSeconds;
    top[FPSTR(_busClkFrequency)] = ioFrequency / 1000;
    DEBUG_PRINTLN(F("4 Line Display config saved."));
  }

  /*
   * readFromConfig() can be used to read back the custom settings you added with addToConfig().
   * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
   *
   * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
   * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
   * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
   */
  bool readFromConfig(JsonObject &root)
  {
    bool needsRedraw = false;
    DisplayType newType = type;
    int8_t oldPin[5];
    for (byte i = 0; i < 5; i++)
      oldPin[i] = ioPin[i];

    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
    {
      DEBUG_PRINT(FPSTR(_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }

    enabled = top[FPSTR(_enabled)] | enabled;
    newType = top["type"] | newType;
    for (byte i = 0; i < 5; i++)
      ioPin[i] = top["pin"][i] | ioPin[i];
    flip = top[FPSTR(_flip)] | flip;
    contrast = top[FPSTR(_contrast)] | contrast;
#ifndef ARDUINO_ARCH_ESP32
    refreshRate = top[FPSTR(_refreshRate)] | refreshRate;
    refreshRate = min(5000, max(250, (int)refreshRate));
#endif
    screenTimeout = (top[FPSTR(_screenTimeOut)] | screenTimeout / 1000) * 1000;
    sleepMode = top[FPSTR(_sleepMode)] | sleepMode;
    clockMode = top[FPSTR(_clockMode)] | clockMode;
    showSeconds = top[FPSTR(_showSeconds)] | showSeconds;
    contrastFix = top[FPSTR(_contrastFix)] | contrastFix;
    if (newType == SSD1306_SPI || newType == SSD1306_SPI64)
      ioFrequency = min(20000, max(500, (int)(top[FPSTR(_busClkFrequency)] | ioFrequency / 1000))) * 1000; // limit frequency
    else
      ioFrequency = min(3400, max(100, (int)(top[FPSTR(_busClkFrequency)] | ioFrequency / 1000))) * 1000; // limit frequency

    DEBUG_PRINT(FPSTR(_name));
    if (!initDone)
    {
      // first run: reading from cfg.json
      type = newType;
      DEBUG_PRINTLN(F(" config loaded."));
    }
    else
    {
      DEBUG_PRINTLN(F(" config (re)loaded."));
      // changing parameters from settings page
      bool pinsChanged = false;
      for (byte i = 0; i < 5; i++)
        if (ioPin[i] != oldPin[i])
        {
          pinsChanged = true;
          break;
        }
      if (pinsChanged || type != newType)
      {
        PinOwner po = PinOwner::UM_FourLineDisplay;
        uint8_t hw_scl = i2c_scl < 0 ? HW_PIN_SCL : i2c_scl;
        uint8_t hw_sda = i2c_sda < 0 ? HW_PIN_SDA : i2c_sda;
        bool isHW = (oldPin[0] == hw_scl && oldPin[1] == hw_sda);
        if (isHW)
          po = PinOwner::HW_I2C;
        pinManager.deallocateMultiplePins((const uint8_t *)oldPin, 2, po);
        type = newType;
        setup();
        needsRedraw |= true;
      }
      knownHour = 99;
      if (needsRedraw && !wakeDisplay())
        redraw(true);
      else
        overlayLogo(3500);
    }
    // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
    return !top[FPSTR(_contrastFix)].isNull();
  }

  /*
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId()
  {
    return USERMOD_ID_FOUR_LINE_DISP;
  }
};

// strings to reduce flash memory usage (used more than twice)
const char FourLineDisplayUsermod::_name[] PROGMEM = "4LineDisplay";
const char FourLineDisplayUsermod::_enabled[] PROGMEM = "enabled";
const char FourLineDisplayUsermod::_contrast[] PROGMEM = "contrast";
const char FourLineDisplayUsermod::_refreshRate[] PROGMEM = "refreshRate-ms";
const char FourLineDisplayUsermod::_screenTimeOut[] PROGMEM = "screenTimeOutSec";
const char FourLineDisplayUsermod::_flip[] PROGMEM = "flip";
const char FourLineDisplayUsermod::_sleepMode[] PROGMEM = "sleepMode";
const char FourLineDisplayUsermod::_clockMode[] PROGMEM = "clockMode";
const char FourLineDisplayUsermod::_showSeconds[] PROGMEM = "showSeconds";
const char FourLineDisplayUsermod::_busClkFrequency[] PROGMEM = "i2c-freq-kHz";
const char FourLineDisplayUsermod::_contrastFix[] PROGMEM = "contrastFix";

FourLineDisplayUsermod *FourLineDisplayUsermod::instance = nullptr;
