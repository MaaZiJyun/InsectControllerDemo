/**
 * @file wificomp.h
 * @brief Wi-Fi 连接与 SoftAP 回退组件。
 */
#pragma once

namespace WifiComp
{

    /** @brief 优先连接外部网络，失败时启动设备 SoftAP。 */
    void enable();

    /** @brief 当前是否运行在板子自建热点的回退模式。 */
    bool isAccessPointMode();
}
