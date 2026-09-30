/**
 * @file wificomp.h
 * @brief Wi-Fi 连接与 SoftAP 回退组件。
 */
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

namespace WifiComp
{

    /** @brief 优先连接外部网络，失败时启动设备 SoftAP。 */
    void enable();

    /** @brief 当前是否运行在板子自建热点的回退模式。 */
    bool isAccessPointMode();

    /** @brief 在热点保持在线时扫描附近 Wi-Fi，并返回 JSON 数组。 */
    bool scanNetworks(String &json);

    /** @brief 从热点模式加入外部 Wi-Fi；成功时保存凭据并延迟关闭热点。 */
    bool connectToExternalWifi(const String &ssid,
                               const String &password,
                               IPAddress &stationAddress);

    /** @brief 处理成功联网后的延迟热点关闭。 */
    void update();
}
