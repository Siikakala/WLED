#pragma once

#include "wled.h"
// #include <U8g2lib.h>
// #include <Arduino.h>

#define WLED_DEBOUNCE_THRESHOLD 50 // only consider button input of at least 50ms as valid (debouncing)
#define INFO_TIMEOUT_MS 2000

class UsermodOledInfo : public Usermod
{

private:
  bool enabled = false;
  bool initDone = false;
  unsigned long m_lastTime = 0;
  unsigned long m_LoopFrequency = 5000;
  unsigned int showCount = 6; // How many times info will be shown before falling back to button only
  const uint16_t NETWORK_MASK = 0xF00;
  const uint16_t SUBNET_MASK = 0x0F0;
  const uint16_t UNIVERSE_MASK = 0x00F;

  bool displayTurnedOff = false;
  bool sleepMode = true;
  unsigned long nextUpdate = 0;
  unsigned long lastRedraw = 0;
  unsigned long overlayUntil = 0;
  int buttonPresses = 0;
  uint16_t refreshRate = 1000;
  uint32_t screenTimeout = 10000;

  String actor = "";
  uint16_t infoTimeout = INFO_TIMEOUT_MS;
  IPAddress knownIp;
  FourLineDisplayUsermod *display;
  UsermodBattery *battery;
  // UsermodBME280 *temp;

  static const char _name[];
  static const char _enabled[];
  static const char _infoTimeOut[];
  const char *empty = '\0';

  // U8G2 u8g2 = U8G2_SSD1306_128X64_NONAME_2_HW_I2C (U8G2_R0, U8X8_PIN_NONE, HW_PIN_SCL, HW_PIN_SDA);

public:
  inline void enable(bool enable) { enabled = enable; }
  inline bool isEnabled() { return enabled; }

  void setup()
  {
    display = (FourLineDisplayUsermod *)usermods.lookup(USERMOD_ID_FOUR_LINE_DISP);
    battery = (UsermodBattery *)usermods.lookup(USERMOD_ID_BATTERY);
    // temp = (UsermodBME280 *)usermods.lookup(USERMOD_ID_BME280);
    m_lastTime = millis();
    initDone = true;
  }

  void connected()
  {
    knownIp = Network.localIP();
  }

  void loop()
  {
    if (!enabled)
      return;
    if (millis() - m_lastTime > m_LoopFrequency && showCount > 0)
    {
      m_lastTime = millis();
      showCount--;
      if (showCount % 2 == 0)
      {
        showActorInfo();
      }
      else
      {
        showArtNetInfo();
      }
    }
  }

  void showActorInfo()
  {
    char s1[32];
    char s4[32];
    snprintf_P(s1, sizeof(s1), PSTR("Actor:"));
    snprintf_P(s4, sizeof(s4), PSTR("%s"), actor.c_str());

    display->overlay(s1, empty, empty, s4, empty, empty, empty, empty, infoTimeout);
  }

  void showArtNetInfo()
  {
    char s1[32];
    char s4[16];
    char s5[16];
    char s6[16];
    snprintf_P(s1, sizeof(s1), PSTR("Art-Net: %i:%i:%i"), (e131Universe & NETWORK_MASK) >> 8, (e131Universe & SUBNET_MASK) >> 4, e131Universe & UNIVERSE_MASK);
    snprintf_P(s4, sizeof(s4), PSTR("Battery: %i%%"), battery->getBatteryLevel());
    // snprintf_P(s5, sizeof(s5), PSTR("Temp: %f C"), temp->getTemperatureC());
    snprintf_P(s6, sizeof(s6), PSTR("IP: %s"), knownIp.toString().c_str());

    display->overlay(s1, cmDNS, empty, empty, empty, s4, s5, s6, infoTimeout);
  }

  bool handleButton(uint8_t b)
  {
    yield();
    if (!enabled || buttonType[b] == BTN_TYPE_NONE || buttonType[b] == BTN_TYPE_RESERVED || buttonType[b] == BTN_TYPE_PIR_SENSOR || buttonType[b] == BTN_TYPE_ANALOG || buttonType[b] == BTN_TYPE_ANALOG_INVERTED || buttonType[b] == BTN_TYPE_SWITCH)
    {
      return false;
    }

    bool handled = false;
    unsigned long now = millis();

    // momentary button logic
    if (isButtonPressed(b))
    { // pressed
      if (!buttonPressedBefore[b])
        buttonPressedTime[b] = now;
      buttonPressedBefore[b] = true;
    }
    else if (!isButtonPressed(b) && buttonPressedBefore[b])
    { // released
      long dur = now - buttonPressedTime[b];
      if (dur < WLED_DEBOUNCE_THRESHOLD)
      {
        buttonPressedBefore[b] = false;
        return handled;
      } // too short "press", debounce
      buttonWaitTime[b] = now;
      buttonPressedBefore[b] = false;
    }
    // if 350ms elapsed since last press/release it is a short press
    if (buttonWaitTime[b] && now - buttonWaitTime[b] > 350)
    {
      if (now - buttonWaitTime[b] > infoTimeout)
      {
        buttonPresses = 0;
      }
      else
      {
        buttonPresses++;
      }
      buttonWaitTime[b] = 0;
      switch (buttonPresses)
      {
      case 1:
        showActorInfo();
        break;
      case 2:
        showArtNetInfo();
        break;
      default:
        buttonPresses = 0;
        display->overlay(empty, empty, empty, empty, empty, empty, empty, empty, 0);
      }
      handled = true;
    }
    return handled;
  }

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");
    JsonArray actorinfo = user.createNestedArray(F("Actor"));
    actorinfo.add(actor);
    JsonObject feedback = user.createNestedObject(F("ArtNetFeedback"));
    JsonObject netinfo = feedback.createNestedObject(F("NetInfo"));
    netinfo["Network"] = (e131Universe & NETWORK_MASK) >> 8;
    netinfo["Subnet"] = (e131Universe & SUBNET_MASK) >> 4;
    netinfo["Universe"] = (e131Universe & UNIVERSE_MASK);
    JsonObject batteryinfo = feedback.createNestedObject(F("Battery"));
    batteryinfo["percentageLeft"] = battery->getBatteryLevel();
    batteryinfo["currentVoltage"] = battery->getVoltage();
    batteryinfo["maxVoltage"] = battery->getMaxBatteryVoltage();
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top["actor"] = actor;
    top["infoTimeout"] = infoTimeout;
  }

  void appendConfigData()
  {
    oappend(SET_F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(SET_F(":actor"));
    oappend(SET_F("',1,'<br /><small><i>Whose control box this is</i></small>');"));
    oappend(String(FPSTR(_infoTimeOut)).c_str());
    oappend(SET_F(":infoTimeout"));
    oappend(SET_F("',2,'<br /><small><i>How long info is displayed after button press</i></small>');"));
  }

  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
    configComplete &= getJsonValue(top["actor"], actor);
    configComplete &= getJsonValue(top["infoTimeout"], infoTimeout);
    return configComplete;
  }

  uint16_t getId()
  {
    return USERMOD_ID_OLEDINFO;
  }
};

const char UsermodOledInfo::_name[] PROGMEM = "OledInfo";
const char UsermodOledInfo::_enabled[] PROGMEM = "enabled";
const char UsermodOledInfo::_infoTimeOut[] PROGMEM = "infoTimeOutms";