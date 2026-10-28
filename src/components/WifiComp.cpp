/**
 * @file WifiComp.cpp
 * @brief Wi-Fi STA/SoftAP 管理与非阻塞配网实现。
 */
#include "WifiComp.h"

#include "../config/WifiConfig.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

namespace
{
enum class ConnectionState : uint8_t
{
    Idle,
    Connecting,
    Connected,
    Failed
};

bool accessPointActive = false;
uint32_t accessPointShutdownAt = 0;
ConnectionState connectionState = ConnectionState::Idle;
String pendingSsid;
String pendingPassword;
String connectionError;
uint32_t connectionStartedAt = 0;
bool connectionAttemptStarted = false;

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
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (static_cast<uint8_t>(character) < 0x20)
            {
                char escaped[7] = {};
                snprintf(escaped, sizeof(escaped), "\\u%04X",
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

const char *stateName()
{
    switch (connectionState)
    {
    case ConnectionState::Connecting: return "connecting";
    case ConnectionState::Connected: return "connected";
    case ConnectionState::Failed: return "failed";
    case ConnectionState::Idle:
    default: return "idle";
    }
}

bool waitForStationConnection(const String &ssid)
{
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startedAt < WifiConfig::WIFI_CONNECT_TIMEOUT_MS)
    {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("WIFI_CONNECTED SSID=%s IP=%s\n",
                      ssid.c_str(), WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.printf("WIFI_FAILED SSID=%s STATUS=%d\n",
                  ssid.c_str(), static_cast<int>(WiFi.status()));
    return false;
}

bool joinExternalWifiAtBoot(const String &ssid, const String &password)
{
    Serial.printf("Connecting to external Wi-Fi: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid.c_str(), password.c_str());
    if (waitForStationConnection(ssid))
    {
        connectionState = ConnectionState::Connected;
        return true;
    }
    WiFi.disconnect(true);
    return false;
}

bool loadSavedCredentials(String &ssid, String &password)
{
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, true))
        return false;
    ssid = preferences.getString(SSID_KEY, "");
    password = preferences.getString(PASSWORD_KEY, "");
    preferences.end();
    return !ssid.isEmpty();
}

bool saveCredentials(const String &ssid, const String &password)
{
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false))
        return false;
    const bool saved = preferences.putString(SSID_KEY, ssid) == ssid.length() &&
                       preferences.putString(PASSWORD_KEY, password) == password.length();
    preferences.end();
    return saved;
}

bool startAccessPoint(bool disconnectStation)
{
    if (accessPointActive)
        return true;

    Serial.printf("Starting access point: %s\n", WifiConfig::AP_SSID);
    if (disconnectStation)
    {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
    }
    else
    {
        WiFi.mode(WIFI_AP_STA);
    }

    if (!WiFi.softAP(WifiConfig::AP_SSID, WifiConfig::AP_PASSWORD))
    {
        Serial.println("Failed to start the access point");
        return false;
    }

    accessPointActive = true;
    WiFi.setSleep(false);
    Serial.printf("Access point ready: http://%s/\n",
                  WiFi.softAPIP().toString().c_str());
    return true;
}

void beginRequestedConnection()
{
    // 转网期间始终保留 SoftAP 管理通道。
    if (!startAccessPoint(false))
    {
        connectionState = ConnectionState::Failed;
        connectionError = "unable to start management access point";
        return;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
    connectionStartedAt = millis();
    connectionAttemptStarted = true;
    Serial.printf("Joining configured Wi-Fi asynchronously: %s\n", pendingSsid.c_str());
}

void finishRequestedConnection()
{
    if (!saveCredentials(pendingSsid, pendingPassword))
        Serial.println("Warning: failed to persist Wi-Fi credentials");

    connectionState = ConnectionState::Connected;
    connectionError = "";
    connectionAttemptStarted = false;
    accessPointShutdownAt = millis() + WifiConfig::AP_SHUTDOWN_DELAY_MS;
    Serial.printf("WIFI_CONNECTED SSID=%s IP=%s; access point will close\n",
                  pendingSsid.c_str(), WiFi.localIP().toString().c_str());
    pendingPassword = "";
}

void failRequestedConnection()
{
    WiFi.disconnect(false, false);
    connectionState = ConnectionState::Failed;
    connectionError = "unable to join Wi-Fi; access point remains active";
    connectionAttemptStarted = false;
    pendingPassword = "";
    Serial.printf("WIFI_FAILED SSID=%s STATUS=%d\n",
                  pendingSsid.c_str(), static_cast<int>(WiFi.status()));
}
} // namespace

void WifiComp::enable()
{
    String savedSsid;
    String savedPassword;
    const bool hasSavedCredentials = loadSavedCredentials(savedSsid, savedPassword);
    if (hasSavedCredentials && joinExternalWifiAtBoot(savedSsid, savedPassword))
        return;

    const String fallbackSsid(WifiConfig::EX_SSID);
    if ((!hasSavedCredentials || savedSsid != fallbackSsid) &&
        joinExternalWifiAtBoot(fallbackSsid, String(WifiConfig::EX_PASSWORD)))
        return;

    connectionState = ConnectionState::Idle;
    Serial.println("Falling back to access point");
    if (!startAccessPoint(true))
        Serial.println("Wi-Fi startup failed");
}

bool WifiComp::isAccessPointMode()
{
    return accessPointActive;
}

bool WifiComp::scanNetworks(String &json)
{
    if (connectionState == ConnectionState::Connecting)
        return false;

    if (accessPointActive)
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
            continue;

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
            continue;

        if (wroteNetwork)
            json += ',';
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

bool WifiComp::requestExternalWifi(const String &ssid, const String &password)
{
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
        connectionState == ConnectionState::Connecting)
    {
        return false;
    }

    pendingSsid = ssid;
    pendingPassword = password;
    connectionError = "";
    connectionState = ConnectionState::Connecting;
    connectionAttemptStarted = false;
    accessPointShutdownAt = 0;
    return true;
}

void WifiComp::statusJson(String &json)
{
    json = "{\"ok\":true,\"connectionState\":\"";
    json += stateName();
    json += "\",\"accessPoint\":{\"active\":";
    json += accessPointActive ? "true" : "false";
    json += ",\"ssid\":";
    appendJsonString(json, WifiConfig::AP_SSID);
    json += ",\"ip\":\"";
    json += accessPointActive ? WiFi.softAPIP().toString() : String("0.0.0.0");
    json += "\"},\"station\":{\"connected\":";
    json += WiFi.status() == WL_CONNECTED ? "true" : "false";
    json += ",\"ssid\":";
    appendJsonString(json, WiFi.status() == WL_CONNECTED ? WiFi.SSID() : pendingSsid);
    json += ",\"ip\":\"";
    json += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("0.0.0.0");
    json += "\"},\"error\":";
    if (connectionError.isEmpty())
        json += "null";
    else
        appendJsonString(json, connectionError);
    json += '}';
}

void WifiComp::update()
{
    if (connectionState == ConnectionState::Connecting)
    {
        if (!connectionAttemptStarted)
            beginRequestedConnection();
        else if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == pendingSsid)
            finishRequestedConnection();
        else if (millis() - connectionStartedAt >= WifiConfig::WIFI_CONNECT_TIMEOUT_MS)
            failRequestedConnection();
    }

    if (!accessPointActive || accessPointShutdownAt == 0 ||
        static_cast<int32_t>(millis() - accessPointShutdownAt) < 0)
    {
        return;
    }

    accessPointShutdownAt = 0;
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_STA);
    accessPointActive = false;
    Serial.println("Access point stopped; station mode active");
}
