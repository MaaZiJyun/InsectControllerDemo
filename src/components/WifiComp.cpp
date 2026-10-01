/**
 * @file wificomp.cpp
 * @brief Wi-Fi 连接与 SoftAP 回退组件实现。
 */
#include "WifiComp.h"

#include "../config/WifiConfig.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

namespace
{
    bool accessPointMode = false;
    unsigned long accessPointShutdownAt = 0;
    constexpr char PREFERENCES_NAMESPACE[] = "wifi-config";
    constexpr char SSID_KEY[] = "ssid";
    constexpr char PASSWORD_KEY[] = "password";

    void appendJsonString(String &output, const String &value)
    {
        output += '"';
        for (size_t index = 0; index < value.length(); ++index)
        {
            const char character = value[index];
            switch (character)
            {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (static_cast<uint8_t>(character) < 0x20)
                {
                    char escaped[7] = {};
                    snprintf(escaped,
                             sizeof(escaped),
                             "\\u%04X",
                             static_cast<unsigned int>(static_cast<uint8_t>(character)));
                    output += escaped;
                }
                else
                {
                    output += character;
                }
                break;
            }
        }
        output += '"';
    }

    bool waitForStationConnection(const String &ssid)
    {
        const unsigned long startedAt = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - startedAt < WifiConfig::WIFI_CONNECT_TIMEOUT_MS)
        {
            delay(250);
            Serial.print('.');
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED)
        {
            const IPAddress address = WiFi.localIP();
            Serial.printf("WIFI_CONNECTED SSID=%s IP=%s\n",
                          ssid.c_str(),
                          address.toString().c_str());
            Serial.printf("Control API: http://%s/config\n", address.toString().c_str());
            Serial.printf("Video URL: http://%s:81/stream\n", address.toString().c_str());
            return true;
        }

        Serial.printf("WIFI_FAILED SSID=%s STATUS=%d\n",
                      ssid.c_str(),
                      static_cast<int>(WiFi.status()));
        return false;
    }

    bool joinExternalWifi(const String &ssid, const String &password)
    {
        Serial.printf("Connecting to external Wi-Fi: %s\n", ssid.c_str());

        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(ssid.c_str(), password.c_str());
        if (waitForStationConnection(ssid))
        {
            accessPointMode = false;
            return true;
        }

        WiFi.disconnect(true);
        return false;
    }

    bool loadSavedCredentials(String &ssid, String &password)
    {
        Preferences preferences;
        if (!preferences.begin(PREFERENCES_NAMESPACE, true))
        {
            return false;
        }
        ssid = preferences.getString(SSID_KEY, "");
        password = preferences.getString(PASSWORD_KEY, "");
        preferences.end();
        return !ssid.isEmpty();
    }

    bool saveCredentials(const String &ssid, const String &password)
    {
        Preferences preferences;
        if (!preferences.begin(PREFERENCES_NAMESPACE, false))
        {
            return false;
        }
        const bool saved = preferences.putString(SSID_KEY, ssid) == ssid.length() &&
                           preferences.putString(PASSWORD_KEY, password) == password.length();
        preferences.end();
        return saved;
    }

    bool createAccessPoint()
    {
        Serial.printf("Starting access point: %s\n", WifiConfig::AP_SSID);

        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);

        if (WiFi.softAP(WifiConfig::AP_SSID, WifiConfig::AP_PASSWORD))
        {
            accessPointMode = true;
            WiFi.setSleep(false);
            Serial.print("Access point IP: ");
            Serial.println(WiFi.softAPIP());
            Serial.printf("Wi-Fi setup URL: http://%s/\n",
                          WiFi.softAPIP().toString().c_str());
            return true;
        }

        Serial.println("Failed to start the access point");
        return false;
    }

}

void WifiComp::enable()
{
    String savedSsid;
    String savedPassword;
    const bool hasSavedCredentials = loadSavedCredentials(savedSsid, savedPassword);
    if (hasSavedCredentials && joinExternalWifi(savedSsid, savedPassword))
    {
        return;
    }

    const String fallbackSsid(WifiConfig::EX_SSID);
    if ((!hasSavedCredentials || savedSsid != fallbackSsid) &&
        joinExternalWifi(fallbackSsid, String(WifiConfig::EX_PASSWORD)))
    {
        return;
    }

    Serial.println("Falling back to access point");
    if (!createAccessPoint())
    {
        Serial.println("Wi-Fi startup failed");
    }
}

bool WifiComp::isAccessPointMode()
{
    return accessPointMode;
}

bool WifiComp::scanNetworks(String &json)
{
    if (!accessPointMode)
    {
        return false;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    const int count = WiFi.scanNetworks(false, true);
    if (count < 0)
    {
        WiFi.scanDelete();
        return false;
    }

    json = "{\"ok\":true,\"networks\":[";
    bool wroteNetwork = false;
    for (int index = 0; index < count; ++index)
    {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty())
        {
            continue;
        }

        bool duplicate = false;
        for (int previous = 0; previous < index; ++previous)
        {
            if (WiFi.SSID(previous) == ssid)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            continue;
        }

        if (wroteNetwork)
        {
            json += ',';
        }
        json += "{\"ssid\":";
        appendJsonString(json, ssid);
        json += ",\"rssi\":";
        json += WiFi.RSSI(index);
        json += ",\"secure\":";
        json += WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? "false" : "true";
        json += '}';
        wroteNetwork = true;
    }
    json += "]}";
    WiFi.scanDelete();
    return true;
}

bool WifiComp::connectToExternalWifi(const String &ssid,
                                     const String &password,
                                     IPAddress &stationAddress)
{
    if (!accessPointMode || ssid.isEmpty() || ssid.length() > 32 || password.length() > 63)
    {
        return false;
    }

    Serial.printf("Joining configured Wi-Fi: %s\n", ssid.c_str());
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());

    if (!waitForStationConnection(ssid))
    {
        WiFi.disconnect(false, false);
        return false;
    }

    if (!saveCredentials(ssid, password))
    {
        Serial.println("Warning: failed to persist Wi-Fi credentials");
    }

    stationAddress = WiFi.localIP();
    accessPointShutdownAt = millis() + 2500;
    return true;
}

void WifiComp::update()
{
    if (!accessPointMode || accessPointShutdownAt == 0 ||
        static_cast<int32_t>(millis() - accessPointShutdownAt) < 0)
    {
        return;
    }

    accessPointShutdownAt = 0;
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_STA);
    accessPointMode = false;
    Serial.println("Access point stopped; station mode active");
}
