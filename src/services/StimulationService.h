/**
 * @file StimulationService.h
 * @brief 通用差分双相刺激服务。
 */
#pragma once

#include <stdint.h>

namespace StimulationService
{
/** @brief 初始化服务，并将所有刺激引脚置为 INPUT 高阻。 */
void enable();

/**
 * @brief 异步启动刺激。pin1/pin2 是 D0-D10 的板级编号，不是原始 GPIO 号。
 * @return 参数合法且请求已入队时返回 true。
 */
bool startStimulation(uint8_t pin1,
                      uint8_t pin2,
                      float frequencyHz,
                      uint32_t durationMs);

/** @brief 异步停止当前刺激，并将所有刺激引脚恢复高阻。 */
bool stopStimulation();
} // namespace StimulationService
