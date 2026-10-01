/**
 * @file CommandService.h
 * @brief 单槽覆盖式命令队列和 Controller HTTP 服务。
 */
#pragma once

#include "Command.h"

namespace CommandService
{
/** @brief 启动配网/控制 HTTP 服务；刺激命令任务按需延迟创建。 */
bool enable();

/** @brief 在 Arduino loop 中处理 HTTP 请求。 */
void update();

/** @brief 用包含波形参数的新命令覆盖当前缓存。 */
bool submit(const CommandRequest &request);
}
