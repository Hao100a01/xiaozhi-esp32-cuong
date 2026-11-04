#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include <driver/spi_master.h>
#include <driver/uart.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <freertos/queue.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "mcp_server.h"
#include "uta_display.h"
#include "uta_media_player.h"
#include "uta_motion.h"
#include "uta_web_server.h"
#include "wifi_board.h"

namespace {
constexpr const char* kTag = "UTA_Robot";
struct RobotCommand {
    char text[24]{};
    int duration_ms = 0;
};

class UtaBootButton : public Button {
public:
    using Button::Button;

    void OnTripleClick(std::function<void()> callback) {
        triple_click_ = std::move(callback);
        button_event_args_t args{};
        args.multiple_clicks.clicks = 3;
        iot_button_register_cb(
            button_handle_, BUTTON_MULTIPLE_CLICK, &args,
            [](void*, void* context) {
                auto* button = static_cast<UtaBootButton*>(context);
                if (button->triple_click_)
                    button->triple_click_();
            },
            this);
    }

private:
    std::function<void()> triple_click_;
};
}  // namespace

class UtaRobot : public WifiBoard {
public:
    UtaRobot() : active_(settings_.Get()), media_(settings_), boot_(BOOT_BUTTON_GPIO) {
        InitializeDisplay();
        media_.SetVideoSink([this](const uint16_t* pixels, int width, int height) {
            display_->UpdateVideo(pixels, width, height);
        });
        robot_commands_ = xQueueCreate(1, sizeof(RobotCommand));
        ESP_ERROR_CHECK(robot_commands_ ? ESP_OK : ESP_ERR_NO_MEM);
        motion_.Start();
        if (active_.robot_enabled)
            InitializeUart();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        uta::WebActions actions;
        actions.robot = [this](const std::string& name, int value) {
            return QueueRobot(name, value);
        };
        actions.servo = [this](int axis, int angle) { return QueueServo(axis, angle); };
        actions.volume = [this](int value) { SetVolume(value); };
        actions.brightness = [this](int value) { SetBrightness(value); };
        actions.restart = [this]() { QueueRestart(); };
        actions.wifi = [this]() {
            stop_robot_ = true;
            Application::GetInstance().Schedule([this]() { EnterWifiConfigMode(); });
        };
        web_ = std::make_unique<uta::WebServer>(settings_, media_, active_, std::move(actions));
        if (!media_.Start()) {
            ESP_LOGE(kTag, "Không tạo được tác vụ phát nhạc");
        }
        if (xTaskCreate([](void* context) { static_cast<UtaRobot*>(context)->RunControls(); },
                        "uta_dieu_khien", 6144, this, 3, &control_task_) != pdPASS) {
            ESP_LOGE(kTag, "Không tạo được tác vụ điều khiển");
        }
        if (touch_ && xTaskCreate(
                          [](void* context) {
                              auto* board = static_cast<UtaRobot*>(context);
                              for (;;) {
                                  board->PollTouch();
                                  vTaskDelay(pdMS_TO_TICKS(20));
                              }
                          },
                          "uta_cam_ung", 4096, this, 1, &touch_task_) != pdPASS) {
            ESP_LOGW(kTag, "Không tạo được tác vụ cảm ứng; vẫn có thể dùng trang web");
        }
        GetBacklight()->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                         Pin(uta::kSpkBclk), Pin(uta::kSpkLrck), Pin(uta::kSpkDout),
                                         Pin(uta::kMicSck), Pin(uta::kMicWs), Pin(uta::kMicDin));
        return &codec;
    }
    Display* GetDisplay() override { return display_.get(); }
    Backlight* GetBacklight() override {
        static PwmBacklight backlight(Pin(uta::kBacklight), false);
        return &backlight;
    }

    void SetNetworkEventCallback(NetworkEventCallback callback) override {
        WifiBoard::SetNetworkEventCallback([this, callback = std::move(callback)](
                                               NetworkEvent event, const std::string& data) {
            if (event == NetworkEvent::Connected || event == NetworkEvent::WifiConfigModeEnter) {
                start_web_ = true;
            }
            if (event == NetworkEvent::Disconnected) {
                stop_robot_ = true;
                media_.Stop();
            }
            if (callback) {
                callback(event, data);
            }
        });
    }

private:
    uta::SettingsStore settings_;
    uta::Configuration active_;
    uta::Motion motion_{active_};
    std::array<std::atomic<int>, 2> servo_pending_{-1, -1};
    uta::MediaPlayer media_;
    UtaBootButton boot_;
    std::unique_ptr<UtaDisplay> display_;
    std::unique_ptr<uta::WebServer> web_;
    spi_device_handle_t touch_ = nullptr;
    QueueHandle_t robot_commands_ = nullptr;
    TaskHandle_t control_task_ = nullptr;
    TaskHandle_t touch_task_ = nullptr;
    std::atomic<bool> start_web_{false};
    std::atomic<bool> stop_robot_{false};
    std::atomic<int64_t> restart_at_{0};
    std::atomic<int> light_mode_{1};
    bool touching_ = false;
    int start_x_ = 0;
    int start_y_ = 0;
    int last_x_ = 0;
    int last_y_ = 0;
    int64_t touch_started_ = 0;
    int64_t last_tap_ = 0;

    gpio_num_t Pin(uta::PinIndex index) const {
        return static_cast<gpio_num_t>(active_.gpio.pins[index]);
    }

    void InitializeDisplay() {
        spi_bus_config_t bus{};
        bus.mosi_io_num = Pin(uta::kLcdMosi);
        bus.miso_io_num = active_.touch_enabled ? Pin(uta::kTouchMiso) : GPIO_NUM_NC;
        bus.sclk_io_num = Pin(uta::kLcdClock);
        bus.quadwp_io_num = GPIO_NUM_NC;
        bus.quadhd_io_num = GPIO_NUM_NC;
        bus.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO));
        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_panel_io_spi_config_t config{};
        config.cs_gpio_num = Pin(uta::kLcdCs);
        config.dc_gpio_num = Pin(uta::kLcdDc);
        config.pclk_hz = 40 * 1000 * 1000;
        config.trans_queue_depth = 10;
        config.lcd_cmd_bits = 8;
        config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &config, &io));
        esp_lcd_panel_dev_config_t device{};
        device.reset_gpio_num = Pin(uta::kLcdReset);
        device.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        device.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &device, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
        display_ = std::make_unique<UtaDisplay>(
            io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeUart() {
        uart_config_t config{};
        config.baud_rate = UTA_CHASSIS_BAUD;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        ESP_ERROR_CHECK(uart_driver_install(UTA_CHASSIS_UART, 256, 256, 0, nullptr, 0));
        ESP_ERROR_CHECK(uart_param_config(UTA_CHASSIS_UART, &config));
        ESP_ERROR_CHECK(uart_set_pin(UTA_CHASSIS_UART, Pin(uta::kChassisTx), Pin(uta::kChassisRx),
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        WriteRobot("x0.0 y0.0");
        WriteRobot("w2");
    }

    void WriteRobot(const char* message) {
        if (active_.motor_driver) {
            const int speed = active_.motor_speed;
            if (std::strcmp(message, "x0.0 y1.0") == 0)
                motion_.Drive(speed, speed);
            else if (std::strcmp(message, "x0.0 y-1.0") == 0)
                motion_.Drive(-speed, -speed);
            else if (std::strcmp(message, "x-1.0 y0.0") == 0)
                motion_.Drive(-speed, speed);
            else if (std::strcmp(message, "x1.0 y0.0") == 0)
                motion_.Drive(speed, -speed);
            else
                motion_.Stop();
            return;
        }
        if (!active_.robot_enabled)
            return;
        uart_write_bytes(UTA_CHASSIS_UART, message, std::strlen(message));
    }

    bool QueueRobot(const std::string& direction, int value) {
        if ((!active_.robot_enabled && !active_.motor_driver) || !control_task_)
            return false;
        if (direction == "dung") {
            stop_robot_ = true;
            return true;
        }
        RobotCommand command;
        const char* text = nullptr;
        if (direction == "tien")
            text = "x0.0 y1.0";
        else if (direction == "lui")
            text = "x0.0 y-1.0";
        else if (direction == "trai")
            text = "x-1.0 y0.0";
        else if (direction == "phai")
            text = "x1.0 y0.0";
        else if (direction == "nhay" && active_.robot_enabled)
            text = "d1";
        else if (direction == "den" && active_.robot_enabled && value >= 1 && value <= 6) {
            // Bộ điều khiển UART tương thích: w2 sáng thường, w3 nhấp nháy, w4…w7 là các hiệu ứng
            // tiếp.
            std::snprintf(command.text, sizeof(command.text), "w%d", value + 1);
            light_mode_ = value;
        } else {
            return false;
        }
        if (text) {
            if (value < 100 || value > 2000)
                return false;
            std::snprintf(command.text, sizeof(command.text), "%s", text);
            command.duration_ms = value;
        }
        return robot_commands_ && xQueueOverwrite(robot_commands_, &command) == pdPASS;
    }

    bool QueueServo(int axis, int angle) {
        if (!control_task_ || !active_.servos_enabled || axis < 0 || axis > 1 ||
            angle < active_.servo_min[axis] || angle > active_.servo_max[axis])
            return false;
        servo_pending_[axis] = angle;
        return true;
    }

    void InitializeTouch() {
        if (!active_.touch_enabled) {
            return;
        }
        spi_device_interface_config_t config{};
        config.clock_speed_hz = 2 * 1000 * 1000;
        config.spics_io_num = Pin(uta::kTouchCs);
        config.queue_size = 1;
        if (spi_bus_add_device(SPI3_HOST, &config, &touch_) != ESP_OK) {
            ESP_LOGW(kTag, "Không khởi tạo được cảm ứng; vẫn dùng được web và nút BOOT");
            touch_ = nullptr;
            return;
        }
        if (Pin(uta::kTouchIrq) != GPIO_NUM_NC) {
            gpio_config_t input{};
            input.pin_bit_mask = 1ULL << Pin(uta::kTouchIrq);
            input.mode = GPIO_MODE_INPUT;
            input.pull_up_en = GPIO_PULLUP_ENABLE;
            ESP_ERROR_CHECK(gpio_config(&input));
        }
    }

    int ReadTouch(uint8_t command) {
        std::array<uint8_t, 3> tx{command, 0, 0};
        std::array<uint8_t, 3> rx{};
        spi_transaction_t transaction{};
        transaction.length = 24;
        transaction.tx_buffer = tx.data();
        transaction.rx_buffer = rx.data();
        if (spi_device_polling_transmit(touch_, &transaction) != ESP_OK)
            return -1;
        return ((rx[1] << 8) | rx[2]) >> 3;
    }

    void PollTouch() {
        if (!touch_)
            return;
        const int pressure = ReadTouch(0xb0);
        if (pressure < 0)
            return;
        const bool down =
            pressure > 100 && pressure < 4095 &&
            (Pin(uta::kTouchIrq) == GPIO_NUM_NC || gpio_get_level(Pin(uta::kTouchIrq)) == 0);
        const int64_t now = esp_timer_get_time();
        if (down) {
            int x = ReadTouch(0xd0);
            int y = ReadTouch(0x90);
            if (x < 0 || y < 0)
                return;
            last_x_ = std::clamp((x - 200) * DISPLAY_WIDTH / 3600, 0, DISPLAY_WIDTH - 1);
            last_y_ = std::clamp((y - 200) * DISPLAY_HEIGHT / 3600, 0, DISPLAY_HEIGHT - 1);
            if (!touching_) {
                start_x_ = last_x_;
                start_y_ = last_y_;
                touch_started_ = now;
            }
            touching_ = true;
        } else if (touching_) {
            touching_ = false;
            const int dx = last_x_ - start_x_;
            const int dy = last_y_ - start_y_;
            if (std::abs(dx) > 50 && std::abs(dx) > std::abs(dy)) {
                Application::GetInstance().Schedule([this, dx]() {
                    GetBacklight()->SetBrightness(
                        std::clamp(GetBacklight()->brightness() + (dx > 0 ? 5 : -5), 5, 100));
                });
            } else if (std::abs(dy) > 50) {
                Application::GetInstance().Schedule([this, dy]() {
                    GetAudioCodec()->SetOutputVolume(
                        std::clamp(GetAudioCodec()->output_volume() + (dy < 0 ? 5 : -5), 0, 100));
                });
            } else if (now - touch_started_ > 800000) {
                ToggleMedia();
            } else if (last_tap_ && now - last_tap_ < 500000) {
                last_tap_ = 0;
                media_.Stop();
                Application::GetInstance().Schedule(
                    []() { Application::GetInstance().ToggleChatState(); });
            } else {
                last_tap_ = now;
            }
        }
    }

    void SetVolume(int value) {
        Application::GetInstance().Schedule(
            [this, value]() { GetAudioCodec()->SetOutputVolume(std::clamp(value, 0, 100)); });
    }
    void SetBrightness(int value) {
        Application::GetInstance().Schedule(
            [this, value]() { GetBacklight()->SetBrightness(std::clamp(value, 5, 100)); });
    }
    void ToggleMedia() {
        if (media_.IsPlaying())
            media_.Pause(!media_.IsPaused());
        else {
            const auto config = settings_.Get();
            media_.PlayRadio(config.radio_url, config.radio_name);
        }
    }

    void InitializeButtons() {
        boot_.OnClick([this]() {
            media_.Stop();
            stop_robot_ = true;
            Application::GetInstance().Schedule([this]() {
                if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
                    EnterWifiConfigMode();
                } else
                    Application::GetInstance().ToggleChatState();
            });
        });
        boot_.OnDoubleClick([this]() { ToggleMedia(); });
        boot_.OnLongPress([this]() {
            media_.Stop();
            stop_robot_ = true;
            Application::GetInstance().Schedule([this]() { EnterWifiConfigMode(); });
        });
        boot_.OnTripleClick([this]() { ShowAccess(); });
        boot_.OnMultipleClick(
            [this]() {
                // Khôi phục mù khi đã lưu chân LCD sai; không xóa khóa máy chủ nhạc.
                auto config = settings_.Get();
                config.gpio = uta::DefaultPins();
                config.touch_enabled = false;
                config.robot_enabled = false;
                config.motor_driver = 0;
                config.servos_enabled = false;
                if (settings_.Save(config))
                    QueueRestart();
            },
            6);
    }

    void QueueRestart() {
        stop_robot_ = true;
        restart_at_ = esp_timer_get_time() + 1500000;
    }

    void ShowAccess() {
        esp_netif_ip_info_t ip{};
        auto* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!netif || esp_netif_get_ip_info(netif, &ip) != ESP_OK || ip.ip.addr == 0) {
            netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
            if (netif)
                esp_netif_get_ip_info(netif, &ip);
        }
        std::array<char, 80> address{};
        std::snprintf(address.data(), address.size(), "http://" IPSTR ":%d", IP2STR(&ip.ip),
                      UTA_HTTP_PORT);
        const std::string message = std::string(address.data()) +
                                    "\nTài khoản: uta\nMật khẩu: " + settings_.Get().web_password;
        Application::GetInstance().Schedule([this, message]() {
            display_->SetChatMessage("system", message.c_str());
            display_->ShowNotification("Trang điều khiển UTA-robot", 10000);
        });
    }

    void RunControls() {
        int64_t stop_at = 0;
        int64_t spectrum_at = 0;
        std::array<uint8_t, 64> responses{};
        for (;;) {
            if (start_web_.exchange(false)) {
                if (web_->Start())
                    ShowAccess();
                else
                    ESP_LOGE(kTag, "Không mở được trang điều khiển HTTP");
            }
            if (stop_robot_.exchange(false)) {
                if (robot_commands_)
                    xQueueReset(robot_commands_);
                WriteRobot("x0.0 y0.0");
                stop_at = 0;
            }
            for (size_t axis = 0; axis < 2; ++axis) {
                const int angle = servo_pending_[axis].exchange(-1);
                if (angle >= 0)
                    motion_.Servo(axis, angle);
            }
            RobotCommand command;
            if (robot_commands_ && xQueueReceive(robot_commands_, &command, 0) == pdTRUE) {
                WriteRobot(command.text);
                if (command.duration_ms > 0) {
                    stop_at = esp_timer_get_time() + command.duration_ms * 1000LL;
                }
            }
            const int64_t now = esp_timer_get_time();
            if (now - spectrum_at >= 100000) {
                const bool visible =
                    media_.IsPlaying() && !media_.IsVideo() &&
                    Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
                display_->UpdateSpectrum(media_.GetSpectrum(), visible);
                if (!media_.IsVideo() ||
                    Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
                    display_->UpdateVideo(nullptr, 0, 0);
                spectrum_at = now;
            }
            if (stop_at && now >= stop_at) {
                WriteRobot("x0.0 y0.0");
                stop_at = 0;
            }
            if (restart_at_.load() && now >= restart_at_) {
                restart_at_ = 0;
                Application::GetInstance().Schedule([]() { Application::GetInstance().Reboot(); });
            }
            // Đọc bỏ phản hồi chưa có bộ phân tích, tránh đầy bộ đệm RX.
            if (active_.robot_enabled)
                uart_read_bytes(UTA_CHASSIS_UART, responses.data(), responses.size(), 0);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();
        if (active_.robot_enabled || active_.motor_driver) {
            const std::array<std::array<const char*, 3>, 6> moves{{
                {"self.chassis.go_forward", "Cho robot đi tới rồi tự dừng", "tien"},
                {"self.chassis.go_back", "Cho robot đi lùi rồi tự dừng", "lui"},
                {"self.chassis.turn_left", "Cho robot rẽ trái rồi tự dừng", "trai"},
                {"self.chassis.turn_right", "Cho robot rẽ phải rồi tự dừng", "phai"},
                {"self.chassis.dance", "Cho robot nhảy múa trong thời gian giới hạn", "nhay"},
                {"self.chassis.stop", "Dừng robot ngay", "dung"},
            }};
            for (const auto& move : moves) {
                if (!active_.robot_enabled && std::strcmp(move[2], "nhay") == 0)
                    continue;
                const std::string direction = move[2];
                mcp.AddTool(
                    move[0], move[1],
                    PropertyList({Property("thoi_gian_ms", kPropertyTypeInteger, 600, 100, 2000)}),
                    [this, direction](const PropertyList& values) -> ReturnValue {
                        return QueueRobot(direction, values["thoi_gian_ms"].value<int>());
                    });
            }
            if (active_.robot_enabled) {
                mcp.AddTool("self.chassis.switch_light_mode",
                            "Đổi hiệu ứng đèn, chọn số từ 1 đến 6",
                            PropertyList({Property("che_do", kPropertyTypeInteger, 1, 1, 6)}),
                            [this](const PropertyList& values) -> ReturnValue {
                                return QueueRobot("den", values["che_do"].value<int>());
                            });
                mcp.AddTool(
                    "self.chassis.get_light_mode", "Xem hiệu ứng đèn đang chọn", PropertyList(),
                    [this](const PropertyList&) -> ReturnValue { return light_mode_.load(); });
            }
        }
        if (active_.servos_enabled) {
            mcp.AddTool("self.robot.set_head", "Đặt góc đầu và cổ, trong giới hạn đã lưu trên web",
                        PropertyList({Property("dau", kPropertyTypeInteger, 90, 0, 180),
                                      Property("co", kPropertyTypeInteger, 90, 0, 180)}),
                        [this](const PropertyList& values) -> ReturnValue {
                            const int head = values["dau"].value<int>();
                            const int neck = values["co"].value<int>();
                            if (head < active_.servo_min[0] || head > active_.servo_max[0] ||
                                neck < active_.servo_min[1] || neck > active_.servo_max[1])
                                return false;
                            return QueueServo(0, head) && QueueServo(1, neck);
                        });
        }
        mcp.AddTool(
            "self.music.play_song", "Tìm và phát nhạc từ máy chủ đã cài trên trang web",
            PropertyList({Property("bai_hat", kPropertyTypeString),
                          Property("ca_si", kPropertyTypeString, "")}),
            [this](const PropertyList& values) -> ToolResult {
                if (!media_.PlaySong(values["bai_hat"].value<std::string>(),
                                     values["ca_si"].value<std::string>())) {
                    return std::unexpected("Chưa phát được nhạc; hãy đợi hội thoại kết thúc");
                }
                return std::string("Đã nhận yêu cầu tìm bài hát");
            });
        mcp.AddTool("self.radio.play_station",
                    "Phát đài Internet: vov1, vov2, vov3, vov5, vov_gt_hn, vov_gt_hcm, "
                    "vov_tieng_anh hoặc tuy_chinh",
                    PropertyList({Property("ma_dai", kPropertyTypeString)}),
                    [this](const PropertyList& values) -> ToolResult {
                        const auto& id = values["ma_dai"].value<std::string>();
                        bool accepted = false;
                        if (id == "tuy_chinh") {
                            const auto config = settings_.Get();
                            accepted = media_.PlayRadio(config.radio_url, config.radio_name);
                        } else {
                            for (const auto& station : uta::kStations) {
                                if (id == station.id)
                                    accepted = media_.PlayRadio(station.url, station.name);
                            }
                        }
                        if (!accepted)
                            return std::unexpected("Đài không hợp lệ hoặc thiết bị đang hội thoại");
                        return std::string("Đã nhận yêu cầu phát đài");
                    });
        mcp.AddTool("self.music.stop", "Dừng phát nhạc hoặc đài Internet", PropertyList(),
                    [this](const PropertyList&) -> ReturnValue {
                        media_.Stop();
                        return true;
                    });
        mcp.AddTool("self.video.play_mp4", "Phát hình và tiếng từ URL MP4 trực tiếp trên thiết bị",
                    PropertyList({Property("url", kPropertyTypeString)}),
                    [this](const PropertyList& values) -> ReturnValue {
                        return media_.PlayVideo(values["url"].value<std::string>());
                    });
    }
};

DECLARE_BOARD(UtaRobot);
