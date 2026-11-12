#pragma once
#include <driver/gpio.h>
#include <driver/uart.h>

// ESP32-S3-N16R8: mic INMP441 và loa MAX98357 dùng hai cổng I2S độc lập.
#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define MIC_WS_PIN GPIO_NUM_4
#define MIC_SCK_PIN GPIO_NUM_5
#define MIC_DIN_PIN GPIO_NUM_6
#define SPK_DOUT_PIN GPIO_NUM_7
#define SPK_BCLK_PIN GPIO_NUM_15
#define SPK_LRCK_PIN GPIO_NUM_16
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BUILTIN_LED_GPIO GPIO_NUM_NC

// Màn hình TFT SPI ST7789 240 x 240; có thể đổi chân trên trang cài đặt.
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_MOSI_PIN GPIO_NUM_40
#define DISPLAY_MISO_PIN GPIO_NUM_11
#define DISPLAY_CLK_PIN GPIO_NUM_39
#define DISPLAY_DC_PIN GPIO_NUM_42
#define DISPLAY_RST_PIN GPIO_NUM_41
#define DISPLAY_CS_PIN GPIO_NUM_2
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_1
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

// ATFUTA Emotion Engine: compile Mimo eyes; GIF remains the runtime default.
#define UTA_ENABLE_MIMO_EYES 1

// Cảm ứng và bộ điều khiển robot là phụ kiện tùy chọn, mặc định tắt.
#define TOUCH_CS_PIN GPIO_NUM_12
#define TOUCH_INT_PIN GPIO_NUM_13
#define UTA_CHASSIS_UART UART_NUM_1
#define UTA_CHASSIS_TX GPIO_NUM_38
#define UTA_CHASSIS_RX GPIO_NUM_48
#define UTA_CHASSIS_BAUD 115200
#define UTA_HTTP_PORT 8080
