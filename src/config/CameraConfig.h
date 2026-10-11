/**
 * @file CameraConfig.h
 * @brief XIAO ESP32S3 摄像头引脚配置。
 */
#pragma once

#include <esp_camera.h>
#include <stdint.h>

namespace CameraConfig
{

    static constexpr int PWDN_GPIO_NUM = -1;
    static constexpr int RESET_GPIO_NUM = -1;
    static constexpr int XCLK_GPIO_NUM = 10;
    static constexpr int SIOD_GPIO_NUM = 40;
    static constexpr int SIOC_GPIO_NUM = 39;

    static constexpr int Y9_GPIO_NUM = 48;
    static constexpr int Y8_GPIO_NUM = 11;
    static constexpr int Y7_GPIO_NUM = 12;
    static constexpr int Y6_GPIO_NUM = 14;
    static constexpr int Y5_GPIO_NUM = 16;
    static constexpr int Y4_GPIO_NUM = 18;
    static constexpr int Y3_GPIO_NUM = 17;
    static constexpr int Y2_GPIO_NUM = 15;

    static constexpr int VSYNC_GPIO_NUM = 38;
    static constexpr int HREF_GPIO_NUM = 47;
    static constexpr int PCLK_GPIO_NUM = 13;

    static constexpr int LED_GPIO_NUM = 21;
    static constexpr int XCLK_FREQ_HZ = 20000000;

    inline camera_config_t create(bool hasPsram, framesize_t allocationFrameSize)
    {
        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_0;
        config.ledc_timer = LEDC_TIMER_0;
        config.pin_d0 = Y2_GPIO_NUM;
        config.pin_d1 = Y3_GPIO_NUM;
        config.pin_d2 = Y4_GPIO_NUM;
        config.pin_d3 = Y5_GPIO_NUM;
        config.pin_d4 = Y6_GPIO_NUM;
        config.pin_d5 = Y7_GPIO_NUM;
        config.pin_d6 = Y8_GPIO_NUM;
        config.pin_d7 = Y9_GPIO_NUM;
        config.pin_xclk = XCLK_GPIO_NUM;
        config.pin_pclk = PCLK_GPIO_NUM;
        config.pin_vsync = VSYNC_GPIO_NUM;
        config.pin_href = HREF_GPIO_NUM;
        config.pin_sccb_sda = SIOD_GPIO_NUM;
        config.pin_sccb_scl = SIOC_GPIO_NUM;
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_JPEG;
        if (hasPsram)
        {
            // Allocate for the largest planned runtime resolution. The sensor
            // is lowered to the requested output size after initialization.
            config.frame_size = allocationFrameSize;
            // A lower numeric value means higher JPEG quality and a larger
            // frame. 18 keeps VGA readable while substantially reducing the
            // amount of data that must pass through the small lwIP TCP window.
            config.jpeg_quality = 18;
            config.fb_count = 2;
            config.grab_mode = CAMERA_GRAB_LATEST;
            config.fb_location = CAMERA_FB_IN_PSRAM;
        }
        else
        {
            config.frame_size = FRAMESIZE_QVGA;
            config.jpeg_quality = 16;
            config.fb_count = 1;
            config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
            config.fb_location = CAMERA_FB_IN_DRAM;
        }
        return config;
    }

}
