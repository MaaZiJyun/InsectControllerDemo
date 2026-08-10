/**
 * @file VideoService.cpp
 * @brief 摄像头 MJPEG HTTP 视频服务实现。
 */
#include "VideoService.h"

#include "../components/CameraComp.h"
#include "../config/ControllerConfig.h"

#include <Arduino.h>
#include <esp_http_server.h>

namespace
{
httpd_handle_t videoServer = nullptr;

constexpr char STREAM_CONTENT_TYPE[] = "multipart/x-mixed-replace;boundary=frame";
constexpr char STREAM_BOUNDARY[] = "\r\n--frame\r\n";
constexpr char STREAM_PART[] = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

esp_err_t streamHandler(httpd_req_t *request)
{
  esp_err_t result = httpd_resp_set_type(request, STREAM_CONTENT_TYPE);
  if (result != ESP_OK)
  {
    return result;
  }

  httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");

  while (result == ESP_OK)
  {
    camera_fb_t *frame = CameraComp::captureFrame();
    if (frame == nullptr)
    {
      return ESP_FAIL;
    }

    char header[80];
    const int headerLength = snprintf(header,
                                      sizeof(header),
                                      STREAM_PART,
                                      static_cast<unsigned int>(frame->len));

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

    CameraComp::releaseFrame(frame);
    delay(1);
  }

  return result;
}

esp_err_t captureHandler(httpd_req_t *request)
{
  camera_fb_t *frame = CameraComp::captureFrame();
  if (frame == nullptr)
  {
    httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera unavailable");
    return ESP_FAIL;
  }

  httpd_resp_set_type(request, "image/jpeg");
  httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  const esp_err_t result = httpd_resp_send(
      request,
      reinterpret_cast<const char *>(frame->buf),
      frame->len);
  CameraComp::releaseFrame(frame);
  return result;
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
  const httpd_uri_t captureUri = {
      .uri = "/capture",
      .method = HTTP_GET,
      .handler = captureHandler,
      .user_ctx = nullptr};

  result = httpd_register_uri_handler(videoServer, &streamUri);
  if (result == ESP_OK)
  {
    result = httpd_register_uri_handler(videoServer, &captureUri);
  }
  if (result != ESP_OK)
  {
    httpd_stop(videoServer);
    videoServer = nullptr;
    Serial.printf("Failed to register video route: 0x%04x\n",
                  static_cast<unsigned int>(result));
    return false;
  }

  Serial.printf("Video stream started on port %u\n", ControllerConfig::VIDEO_PORT);
  return true;
}

void VideoService::disable()
{
  if (videoServer != nullptr)
  {
    httpd_stop(videoServer);
    videoServer = nullptr;
  }
}

bool VideoService::isEnabled()
{
  return videoServer != nullptr;
}
