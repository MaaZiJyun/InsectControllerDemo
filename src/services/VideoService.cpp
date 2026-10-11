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
#include <inttypes.h>
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
uint32_t publicStreamId = 0;
uint32_t publicFramesSent = 0;
uint64_t publicJpegBytesSent = 0;
uint64_t publicCaptureTimeMs = 0;
uint64_t publicSendTimeMs = 0;
bool publicStreamActive = false;
portMUX_TYPE streamStateMux = portMUX_INITIALIZER_UNLOCKED;

struct StreamTaskContext
{
  httpd_req_t *request;
  uint32_t session;
};

constexpr char STREAM_CONTENT_TYPE[] = "multipart/x-mixed-replace;boundary=frame";
constexpr char STREAM_PART_HEADER[] =
    "\r\n--frame\r\n"
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n\r\n";

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
  uint32_t capturedFrames = 0;
  uint32_t sendAttempts = 0;
  uint32_t maximumCaptureMs = 0;
  uint32_t maximumSendMs = 0;
  uint32_t slowCaptureCount = 0;
  uint32_t slowSendCount = 0;
  uint64_t totalCaptureMs = 0;
  uint64_t totalSendMs = 0;
  uint64_t totalJpegBytes = 0;
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
    totalCaptureMs += captureMs;
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
      ++capturedFrames;
      const size_t frameLength = frame->len;
      // Send the boundary and MIME headers together. Keeping these in separate
      // chunks creates two tiny TCP writes for every JPEG frame.
      char partHeader[112];
      const int partHeaderLength = snprintf(partHeader,
                                            sizeof(partHeader),
                                            STREAM_PART_HEADER,
                                            static_cast<unsigned int>(frameLength));

      const uint32_t sendStartedAt = millis();
      ++sendAttempts;
      if (partHeaderLength <= 0 ||
          static_cast<size_t>(partHeaderLength) >= sizeof(partHeader))
      {
        result = ESP_FAIL;
      }
      else
      {
        result = httpd_resp_send_chunk(request,
                                       partHeader,
                                       static_cast<size_t>(partHeaderLength));
      }
      if (result == ESP_OK)
      {
        result = httpd_resp_send_chunk(
            request,
            reinterpret_cast<const char *>(frame->buf),
            frameLength);
      }
      const uint32_t sendMs = millis() - sendStartedAt;
      totalSendMs += sendMs;
      maximumSendMs = max(maximumSendMs, sendMs);
      if (sendMs >= 100)
      {
        ++slowSendCount;
      }

      CameraComp::releaseFrame(frame);
      portENTER_CRITICAL(&streamStateMux);
      if (activeStreamSession == session && publicStreamActive)
      {
        publicCaptureTimeMs = totalCaptureMs;
        publicSendTimeMs = totalSendMs;
      }
      portEXIT_CRITICAL(&streamStateMux);
      if (result == ESP_OK)
      {
        ++sentFrames;
        totalJpegBytes += frameLength;
        portENTER_CRITICAL(&streamStateMux);
        if (activeStreamSession == session && publicStreamActive)
        {
          ++publicFramesSent;
          publicJpegBytesSent = totalJpegBytes;
        }
        portEXIT_CRITICAL(&streamStateMux);
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
      // Always block for at least one scheduler tick. taskYIELD() only yields
      // to tasks of equal or higher priority and could starve Arduino loopTask.
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  const uint64_t averageFrameBytes = sentFrames == 0 ? 0 : totalJpegBytes / sentFrames;
  const uint64_t averageCaptureMs = capturedFrames == 0 ? 0 : totalCaptureMs / capturedFrames;
  const uint64_t averageSendMs = sendAttempts == 0 ? 0 : totalSendMs / sendAttempts;
  Serial.printf("MJPEG session %lu ended: frames=%lu jpegBytes=%" PRIu64
                " avgFrame=%" PRIu64 "B result=0x%04x current=%s "
                "captureTotal=%" PRIu64 "ms captureAvg=%" PRIu64
                "ms captureMax=%lums captureSlow=%lu "
                "sendTotal=%" PRIu64 "ms sendAvg=%" PRIu64
                "ms sendMax=%lums sendSlow=%lu RSSI=%d\n",
                static_cast<unsigned long>(session),
                static_cast<unsigned long>(sentFrames),
                totalJpegBytes,
                averageFrameBytes,
                static_cast<unsigned int>(result),
                isCurrentStream(session) ? "true" : "false",
                totalCaptureMs,
                averageCaptureMs,
                static_cast<unsigned long>(maximumCaptureMs),
                static_cast<unsigned long>(slowCaptureCount),
                totalSendMs,
                averageSendMs,
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
    publicStreamActive = false;
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
  if (++publicStreamId == 0)
  {
    ++publicStreamId;
  }
  publicFramesSent = 0;
  publicJpegBytesSent = 0;
  publicCaptureTimeMs = 0;
  publicSendTimeMs = 0;
  publicStreamActive = true;
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
                              1,
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
      publicStreamActive = false;
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
  // Abort a stalled client promptly instead of letting one JPEG hold the
  // framebuffer and stream worker for several seconds.
  config.send_wait_timeout = 2;
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

  // A send can wait for the configured two-second socket timeout. Leave a
  // small margin so camera changes never race an in-flight frame.
  for (uint16_t attempt = 0; attempt < 250 && streamCameraUsers > 0; ++attempt)
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
  publicStreamActive = false;
  portEXIT_CRITICAL(&streamStateMux);

  if (server != nullptr && streamSocket >= 0)
  {
    httpd_sess_trigger_close(server, streamSocket);
  }

  // The worker normally exits immediately after its socket is closed. Bound
  // the wait so camera reconfiguration never races a frame still being sent.
  for (uint16_t attempt = 0; attempt < 250 && activeStreamWorkers > 0; ++attempt)
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

VideoService::StreamStats VideoService::stats()
{
  StreamStats snapshot = {};
  portENTER_CRITICAL(&streamStateMux);
  snapshot.active = publicStreamActive;
  snapshot.streamId = publicStreamId;
  snapshot.framesSent = publicFramesSent;
  snapshot.jpegBytesSent = publicJpegBytesSent;
  snapshot.captureTimeMs = publicCaptureTimeMs;
  snapshot.sendTimeMs = publicSendTimeMs;
  portEXIT_CRITICAL(&streamStateMux);
  return snapshot;
}
