/**
 * @file CommandService.h
 * @brief 单槽覆盖式命令队列和 Controller HTTP 服务。
 */
#pragma once

#include "Command.h"

namespace CommandService
{
/** @brief 启动控制页面 HTTP 服务和命令执行任务。 */
bool enable();

/** @brief 在 Arduino loop 中处理控制页面 HTTP 请求。 */
void update();

/** @brief 覆盖当前缓存命令。 */
bool submit(Command command);
}
