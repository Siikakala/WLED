#pragma once

#include "wled.h"

class UsermodOledInfo : public Usermod
{

private:
  unsigned long m_lastTime = 0;
  unsigned long m_LoopFrequency = 45000;
  const uint16_t NETWORK_MASK = 0xF00;
  const uint16_t SUBNET_MASK = 0x0F0;
  const uint16_t UNIVERSE_MASK = 0x00F;
  IPAddress knownIp;
  FourLineDisplayUsermod *display;
  UsermodBattery *battery;

public:
  void setup()
  {
    display = (FourLineDisplayUsermod *)usermods.lookup(USERMOD_ID_FOUR_LINE_DISP);
    battery = (UsermodBattery *)usermods.lookup(USERMOD_ID_BATTERY);
  }

  void connected()
  {
    knownIp = Network.localIP();
  }

  void loop()
  {
    if (millis() - m_lastTime > m_LoopFrequency)
    {
      m_lastTime = millis();
      char s1[32];
      char s2[16];
      char s3[16];
      snprintf_P(s1, sizeof(s1), PSTR("Art-Net: %i:%i:%i"), (e131Universe & NETWORK_MASK) >> 8, (e131Universe & SUBNET_MASK) >> 4, e131Universe & UNIVERSE_MASK);
      snprintf_P(s2, sizeof(s2), PSTR("Battery: %i%%"), battery->getBatteryLevel());
      snprintf_P(s3, sizeof(s3), PSTR("IP: %s"), knownIp.toString().c_str());
      
      display->overlay(s1, cmDNS, s2, s3, 15000);
    }
  }

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");
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

  uint16_t getId()
  {
    return USERMOD_ID_OLEDINFO;
  }
};