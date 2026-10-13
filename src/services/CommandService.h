/**
 * @file CommandService.h
 * @brief Controller HTTP 服务。
 */
#pragma once

namespace CommandService
{
/** @brief 启动配网/控制 HTTP 服务。 */
bool enable();

/** @brief 在 Arduino loop 中处理 HTTP 请求。 */
void update();

}
