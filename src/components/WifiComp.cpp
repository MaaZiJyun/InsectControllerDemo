/**
 * @file wificomp.cpp
 * @brief Wi-Fi 连接与 SoftAP 回退组件实现。
 */
#include "WifiComp.h"

#include "../config/WifiConfig.h"

#include <Arduino.h>
#include <WiFi.h>

namespace
{

    // bool joinExternalWifi()
    // {
    //     Serial.printf("Connecting to external Wi-Fi: %s\n", WifiConfig::EX_SSID);

    //     WiFi.mode(WIFI_STA);
    //     WiFi.begin(WifiConfig::EX_SSID, WifiConfig::EX_PASSWORD);

    //     const unsigned long startedAt = millis();
    //     while (WiFi.status() != WL_CONNECTED &&
    //            millis() - startedAt < WifiConfig::WIFI_CONNECT_TIMEOUT_MS)
    //     {
    //         delay(250);
    //         Serial.print('.');
    //     }
    //     Serial.println();

    //     if (WiFi.status() == WL_CONNECTED)
    //     {
    //         Serial.printf("Connected to %s\n", WifiConfig::EX_SSID);
    //         Serial.print("Station IP: ");
    //         Serial.println(WiFi.localIP());
    //         return true;
    //     }

    //     Serial.printf("Could not connect to %s\n", WifiConfig::EX_SSID);
    //     return false;
    // }

    bool createAccessPoint()
    {
        Serial.printf("Starting access point: %s\n", WifiConfig::AP_SSID);

        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);

        if (WiFi.softAP(WifiConfig::AP_SSID, WifiConfig::AP_PASSWORD))
        {
            Serial.print("Access point IP: ");
            Serial.println(WiFi.softAPIP());
            return true;
        }

        Serial.println("Failed to start the access point");
        return false;
    }

}

void WifiComp::enable()
{
    // if (!joinExternalWifi())
    // {
    //     createAccessPoint();
    // }
    createAccessPoint();
}
