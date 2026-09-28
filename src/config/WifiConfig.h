/**
 * @file WifiConfig.h
 * @brief 设备 SoftAP 的网络凭据配置。
 *
 * 凭据会编译进固件；在生产部署中应通过受控配置流程管理，避免公开传播。
 */
#pragma once

namespace WifiConfig
{

    static constexpr long WIFI_CONNECT_TIMEOUT_MS = 10000;

    static constexpr char AP_SSID[] = "XIAOESP32S3";      /// 网络名称。
    static constexpr char AP_PASSWORD[] = "xiaoesp32s3";  /// 访问密码。

    static constexpr char EX_SSID[] = "MZY";     /// 外部网络名称。
    static constexpr char EX_PASSWORD[] = "96054718";           /// 外部网络访问密码。

}
