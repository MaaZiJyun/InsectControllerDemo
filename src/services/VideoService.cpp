/**
 * @file VideoService.cpp
 * @brief 摄像头 MJPEG HTTP 视频服务实现。
 */
#include "VideoService.h"

#include "../components/CameraComp.h"
#include "../config/ControllerConfig.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>
#include <stdlib.h>

namespace
{
httpd_handle_t videoServer = nullptr;
volatile bool stopRequested = false;
uint32_t streamGeneration = 0;
volatile uint32_t activeStreamSession = 0;
volatile uint8_t activeStreamWorkers = 0;
volatile int activeStreamSocket = -1;
volatile bool pauseRequested = false;
volatile uint8_t streamCameraUsers = 0;
portMUX_TYPE streamStateMux = portMUX_INITIALIZER_UNLOCKED;

struct StreamTaskContext
{
  httpd_req_t *request;
  uint32_t session;
};

constexpr char STREAM_CONTENT_TYPE[] = "multipart/x-mixed-replace;boundary=frame";
constexpr char STREAM_BOUNDARY[] = "\r\n--frame\r\n";
constexpr char STREAM_PART[] = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

bool isCurrentStream(uint32_t session)
{
  return !stopRequested && session == activeStreamSession;
}

void streamTask(void *parameter)
{
  StreamTaskContext *context = static_cast<StreamTaskContext *>(parameter);
  httpd_req_t *request = context->request;
  const uint32_t session = context->session;
  const int socket = httpd_req_to_sockfd(request);

  // MJPEG consists of many small writes. Disable Nagle buffering so a boundary
  // or header is not held while TCP waits for another frame fragment.
  int enabled = 1;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));

  esp_err_t result = httpd_resp_set_type(request, STREAM_CONTENT_TYPE);
  if (result == ESP_OK)
  {
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store, no-cache, must-revalidate");
    httpd_resp_set_hdr(request, "Pragma", "no-cache");
  }

  uint32_t nextFrameAt = millis();
  uint32_t sentFrames = 0;
  uint32_t maximumCaptureMs = 0;
  uint32_t maximumSendMs = 0;
  uint32_t slowCaptureCount = 0;
  uint32_t slowSendCount = 0;
  while (result == ESP_OK && isCurrentStream(session))
  {
    bool mayUseCamera = false;
    portENTER_CRITICAL(&streamStateMux);
    if (!pauseRequested && isCurrentStream(session))
    {
      ++streamCameraUsers;
      mayUseCamera = true;
    }
    portEXIT_CRITICAL(&streamStateMux);

    if (!mayUseCamera)
    {
      // A still photo temporarily owns the camera. Keep this HTTP request and
      // socket alive so the browser resumes without replacing its <img> src.
      nextFrameAt = millis();
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    const uint32_t captureStartedAt = millis();
    camera_fb_t *frame = CameraComp::captureFrame();
    const uint32_t captureMs = millis() - captureStartedAt;
    maximumCaptureMs = max(maximumCaptureMs, captureMs);
    if (captureMs >= 100)
    {
      ++slowCaptureCount;
    }
    if (frame == nullptr)
    {
      result = ESP_FAIL;
    }
    else
    {
      char header[80];
      const int headerLength = snprintf(header,
                                        sizeof(header),
                                        STREAM_PART,
                                        static_cast<unsigned int>(frame->len));

      const uint32_t sendStartedAt = millis();
      result = httpd_resp_send_chunk(request, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
      if (result == ESP_OK && headerLength > 0)
      {
        result = httpd_resp_send_chunk(request, header, static_cast<size_t>(headerLength));
      }
      if (result == ESP_OK)
      {
        result = httpd_resp_send_chunk(
            request,
            reinterpret_cast<const char *>(frame->buf),
            frame->len);
      }
      const uint32_t sendMs = millis() - sendStartedAt;
      maximumSendMs = max(maximumSendMs, sendMs);
      if (sendMs >= 100)
      {
        ++slowSendCount;
      }

      CameraComp::releaseFrame(frame);
      if (result == ESP_OK)
      {
        ++sentFrames;
      }
    }

    portENTER_CRITICAL(&streamStateMux);
    if (streamCameraUsers > 0)
    {
      --streamCameraUsers;
    }
    portEXIT_CRITICAL(&streamStateMux);

    if (result != ESP_OK)
    {
      break;
    }

    // Use an absolute deadline. This prevents capture/send time from being
    // added to every interval and accumulating visible pacing drift.
    const uint8_t fps = CameraComp::targetFps();
    const uint32_t frameIntervalMs = (1000U + fps - 1U) / fps;
    nextFrameAt += frameIntervalMs;
    const int32_t waitMs = static_cast<int32_t>(nextFrameAt - millis());
    if (waitMs > 0)
    {
      vTaskDelay(pdMS_TO_TICKS(waitMs));
    }
    else
    {
      // The camera or network missed the deadline. Drop schedule debt and ask
      // CAMERA_GRAB_LATEST for the newest available frame immediately.
      nextFrameAt = millis();
      taskYIELD();
    }
  }

  Serial.printf("MJPEG session %lu ended: frames=%lu result=0x%04x current=%s "
                "captureMax=%lums captureSlow=%lu sendMax=%lums sendSlow=%lu RSSI=%d\n",
                static_cast<unsigned long>(session),
                static_cast<unsigned long>(sentFrames),
                static_cast<unsigned int>(result),
                isCurrentStream(session) ? "true" : "false",
                static_cast<unsigned long>(maximumCaptureMs),
                static_cast<unsigned long>(slowCaptureCount),
                static_cast<unsigned long>(maximumSendMs),
                static_cast<unsigned long>(slowSendCount),
                WiFi.RSSI());

  httpd_req_async_handler_complete(request);

  portENTER_CRITICAL(&streamStateMux);
  if (activeStreamWorkers > 0)
  {
    --activeStreamWorkers;
  }
  if (activeStreamSession == session)
  {
    activeStreamSocket = -1;
  }
  portEXIT_CRITICAL(&streamStateMux);

  free(context);
  vTaskDelete(nullptr);
}

esp_err_t streamHandler(httpd_req_t *request)
{
  httpd_req_t *asyncRequest = nullptr;
  esp_err_t result = httpd_req_async_handler_begin(request, &asyncRequest);
  if (result != ESP_OK)
  {
    return result;
  }

  StreamTaskContext *context = static_cast<StreamTaskContext *>(malloc(sizeof(StreamTaskContext)));
  if (context == nullptr)
  {
    httpd_req_async_handler_complete(asyncRequest);
    return ESP_ERR_NO_MEM;
  }

  int previousSocket = -1;
  portENTER_CRITICAL(&streamStateMux);
  previousSocket = activeStreamSocket;
  context->request = asyncRequest;
  context->session = ++activeStreamSession;
  activeStreamSocket = httpd_req_to_sockfd(asyncRequest);
  ++activeStreamWorkers;
  portEXIT_CRITICAL(&streamStateMux);

  // A browser can reconnect before its old <img> request has closed. Keep one
  // producer only and terminate the stale socket instead of queuing latency.
  if (previousSocket >= 0 && videoServer != nullptr)
  {
    httpd_sess_trigger_close(videoServer, previousSocket);
  }

  TaskHandle_t task = nullptr;
  const uint32_t session = context->session;
  if (xTaskCreatePinnedToCore(streamTask,
                              "mjpeg-stream",
                              8192,
                              context,
                              2,
                              &task,
                              1) != pdPASS)
  {
    httpd_req_async_handler_complete(asyncRequest);
    free(context);
    portENTER_CRITICAL(&streamStateMux);
    if (activeStreamWorkers > 0)
    {
      --activeStreamWorkers;
    }
    if (activeStreamSession == session)
    {
      activeStreamSocket = -1;
    }
    portEXIT_CRITICAL(&streamStateMux);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

} // namespace

bool VideoService::enable()
{
  if (videoServer != nullptr)
  {
    return true;
  }
  if (!CameraComp::isEnabled())
  {
    Serial.println("Cannot start video service: camera is not initialized");
    return false;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = ControllerConfig::VIDEO_PORT;
  config.max_open_sockets = 4;
  config.lru_purge_enable = true;
  config.stack_size = 8192;
  config.send_wait_timeout = 5;
  config.recv_wait_timeout = 1;

  stopRequested = false;
  pauseRequested = false;
  esp_err_t result = httpd_start(&videoServer, &config);
  if (result != ESP_OK)
  {
    videoServer = nullptr;
    Serial.printf("Failed to start video server: 0x%04x\n",
                  static_cast<unsigned int>(result));
    return false;
  }

  const httpd_uri_t streamUri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = streamHandler,
      .user_ctx = nullptr};
  result = httpd_register_uri_handler(videoServer, &streamUri);
  if (result != ESP_OK)
  {
    httpd_stop(videoServer);
    videoServer = nullptr;
    Serial.printf("Failed to register video route: 0x%04x\n",
                  static_cast<unsigned int>(result));
    return false;
  }

  ++streamGeneration;
  Serial.printf("Video stream started on port %u\n", ControllerConfig::VIDEO_PORT);
  return true;
}

bool VideoService::pauseForCameraChange()
{
  if (videoServer == nullptr)
  {
    return false;
  }

  portENTER_CRITICAL(&streamStateMux);
  pauseRequested = true;
  portEXIT_CRITICAL(&streamStateMux);

  // A send can wait for the configured five-second socket timeout. Usually the
  // current small frame finishes immediately; this bound handles a broken link.
  for (uint16_t attempt = 0; attempt < 550 && streamCameraUsers > 0; ++attempt)
  {
    delay(10);
  }
  return streamCameraUsers == 0;
}

void VideoService::resumeAfterCameraChange()
{
  portENTER_CRITICAL(&streamStateMux);
  pauseRequested = false;
  portEXIT_CRITICAL(&streamStateMux);
}

void VideoService::disable()
{
  stopRequested = true;
  pauseRequested = false;
  httpd_handle_t server = videoServer;
  int streamSocket = -1;
  portENTER_CRITICAL(&streamStateMux);
  ++activeStreamSession;
  streamSocket = activeStreamSocket;
  portEXIT_CRITICAL(&streamStateMux);

  if (server != nullptr && streamSocket >= 0)
  {
    httpd_sess_trigger_close(server, streamSocket);
  }

  // The worker normally exits immediately after its socket is closed. Bound
  // the wait so camera reconfiguration never races a frame still being sent.
  for (uint16_t attempt = 0; attempt < 550 && activeStreamWorkers > 0; ++attempt)
  {
    delay(10);
  }

  if (server != nullptr)
  {
    httpd_stop(server);
  }
  videoServer = nullptr;
}

bool VideoService::isEnabled()
{
  return videoServer != nullptr;
}

uint32_t VideoService::generation()
{
  return streamGeneration;
}
