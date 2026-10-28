/**
 * @file WifiComp.h
 * @brief Wi-Fi STA/SoftAP 管理与非阻塞配网。
 */
#pragma once

#include <Arduino.h>

namespace WifiComp
{
void enable();
bool isAccessPointMode();
bool scanNetworks(String &json);

/** @brief 提交新 STA 凭据；连接由 update() 非阻塞推进。 */
bool requestExternalWifi(const String &ssid, const String &password);

/** @brief 生成不包含密码的 Wi-Fi 状态 JSON。 */
void statusJson(String &json);

void update();
} // namespace WifiComp
