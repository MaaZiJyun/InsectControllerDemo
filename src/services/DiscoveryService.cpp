/**
 * @file DiscoveryService.cpp
 * @brief 响应 Exp.-Recorder 的 UDP 局域网发现协议。
 */
#include "DiscoveryService.h"

#include "../config/ControllerConfig.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_mac.h>
#include <string.h>

namespace
{
WiFiUDP discoveryUdp;
bool discoveryEnabled = false;

constexpr char DISCOVERY_REQUEST[] = "EXP_RECORDER_DISCOVER_V1";
constexpr char DISCOVERY_PROTOCOL[] = "EXP_RECORDER_BOARD_V1";

void formatStationMac(char output[18])
{
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(output,
           18,
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
} // namespace

bool DiscoveryService::enable()
{
  if (discoveryEnabled)
  {
    return true;
  }

  discoveryEnabled = discoveryUdp.begin(ControllerConfig::DISCOVERY_PORT) == 1;
  if (!discoveryEnabled)
  {
    Serial.printf("Failed to start discovery service on UDP port %u\n",
                  ControllerConfig::DISCOVERY_PORT);
    return false;
  }

  Serial.printf("Discovery service started on UDP port %u\n",
                ControllerConfig::DISCOVERY_PORT);
  return true;
}

void DiscoveryService::update()
{
  if (!discoveryEnabled)
  {
    return;
  }

  const int packetSize = discoveryUdp.parsePacket();
  if (packetSize <= 0)
  {
    return;
  }

  char request[64] = {};
  const int bytesRead = discoveryUdp.read(
      reinterpret_cast<uint8_t *>(request), sizeof(request) - 1);
  if (bytesRead <= 0 || strcmp(request, DISCOVERY_REQUEST) != 0)
  {
    return;
  }

  char mac[18] = {};
  formatStationMac(mac);

  char response[224] = {};
  snprintf(response,
           sizeof(response),
           "{\"protocol\":\"%s\",\"mac\":\"%s\",\"model\":\"XIAO ESP32S3 Sense\",\"control_port\":%u,\"video_port\":%u}",
           DISCOVERY_PROTOCOL,
           mac,
           ControllerConfig::CONTROL_PORT,
           ControllerConfig::VIDEO_PORT);

  discoveryUdp.beginPacket(discoveryUdp.remoteIP(), discoveryUdp.remotePort());
  discoveryUdp.write(reinterpret_cast<const uint8_t *>(response), strlen(response));
  discoveryUdp.endPacket();
}
