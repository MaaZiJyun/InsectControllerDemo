/**
 * @file DiscoveryService.h
 * @brief Exp.-Recorder 局域网板卡发现服务。
 */
#pragma once

namespace DiscoveryService
{
/** @brief 在 UDP 发现端口上开始监听。 */
bool enable();

/** @brief 非阻塞处理待响应的发现广播。 */
void update();
}
