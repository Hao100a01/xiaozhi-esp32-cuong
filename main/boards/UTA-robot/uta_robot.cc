#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <driver/uart.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_gc9a01.h>
#include <esp_lcd_ili9341.h>
#include <esp_lcd_panel_sh1106.h>
#include <esp_lcd_st7735.h>
#include <esp_lvgl_port.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <freertos/queue.h>

#include <wifi_manager.h>
#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "display/oled_display.h"
#include "mcp_server.h"
#include "uta_cob_lights.h"
#include "uta_display.h"
#include "uta_location.h"
#include "uta_media_mcp.h"
#include "uta_media_player.h"
#include "uta_motion.h"
#include "uta_range_sensors.h"
#include "uta_reply_gesture.h"
#include "uta_weather.h"
#include "uta_web_server.h"
#include "wifi_board.h"

namespace {
constexpr const char* kTag = "UTA_Robot";
struct RobotCommand {
    char text[24]{};
    int duration_ms = 0;
    bool dance = false;
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
    void OnAssistantSentence(const std::string& sentence) override {
        if (active_.servos_enabled[0] || active_.servos_enabled[1]) {
            const auto gesture = uta::ReplyGestureFor(sentence);
            if ((gesture == uta::ReplyGesture::Nod && active_.servos_enabled[0]) ||
                (gesture == uta::ReplyGesture::Shake && active_.servos_enabled[1]))
                pending_gesture_.store(gesture);
        }
    }

    UtaRobot()
        : active_(settings_.Get()), media_(settings_),
          boot_(uta::UsesGpio(active_, BOOT_BUTTON_GPIO) ? GPIO_NUM_NC : BOOT_BUTTON_GPIO) {
        InitializeDisplay();
        range_.Start(oled_buses_);
        media_.SetVideoSink([this](const uint16_t* pixels, int width, int height) {
            if (display_)
                display_->UpdateVideo(pixels, width, height);
        });
        robot_commands_ = xQueueCreate(1, sizeof(RobotCommand));
        ESP_ERROR_CHECK(robot_commands_ ? ESP_OK : ESP_ERR_NO_MEM);
        motion_.Start();
        cob_.Start();
        if ((cob_.Available(uta::kRgbLight) || cob_.Available(uta::kEdisonLight) ||
             cob_.Available(uta::kStripLight)) &&
            xTaskCreate([](void* context) { static_cast<UtaRobot*>(context)->RunCobLights(); },
                        "uta_cob", 3072, this, 2, &cob_task_) != pdPASS)
            ESP_LOGW(kTag, "Không tạo được tác vụ đèn COB");
        if (active_.robot_enabled)
            InitializeUart();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        uta::WebActions actions;
        actions.robot = [this](const std::string& name, int value) {
            return QueueRobot(name, value);
        };
        actions.cob_available = [this](int light) { return cob_task_ && cob_.Available(light); };
        actions.cob = [this](int light, int value) { return QueueCob(light, value); };
        actions.tof_mm = [this]() { return range_.TofMillimeters(); };
        actions.ultrasonic_mm = [this]() { return range_.UltrasonicMillimeters(); };
        actions.servo = [this](int axis, int angle) { return QueueServo(axis, angle); };
        actions.extra_servo = [this](int axis, int value) {
            if (!control_task_ || restart_at_.load() || axis < 0 ||
                static_cast<size_t>(axis) >= active_.extra_servo_pins.size() ||
                active_.extra_servo_pins[axis] < 0)
                return false;
            extra_servo_pending_[axis].store(value);
            return true;
        };
        actions.volume = [this](int value) { SetVolume(value); };
        actions.brightness = [this](int value) { SetBrightness(value); };
        actions.rotation = [this]() { return display_ ? display_->Rotation() : 0; };
        actions.set_rotation = [this](int degrees) { return display_ && display_->SetRotation(degrees); };
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
        if (auto* backlight = GetBacklight())
            backlight->RestoreBrightness();
        if (!uta::LocationService::GetInstance().Start())
            ESP_LOGW(kTag, "Không tạo được tác vụ định vị IP");
        if (!uta::WeatherService::GetInstance().Start())
            ESP_LOGW(kTag, "Không tạo được tác vụ thời tiết");
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                         Pin(uta::kSpkBclk), Pin(uta::kSpkLrck), Pin(uta::kSpkDout),
                                         Pin(uta::kMicSck), Pin(uta::kMicWs), Pin(uta::kMicDin));
        return &codec;
    }
    Display* GetDisplay() override { return display_ ? display_.get() : compact_display_.get(); }
    Backlight* GetBacklight() override {
        if (active_.primary_lcd.driver == uta::LcdDriver::Off ||
            uta::IsOled(active_.primary_lcd.driver))
            return nullptr;
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
    uta::RangeSensors range_{active_};
    uta::CobLights cob_{active_};
    static constexpr int kCobNoCommand = -1;
    static constexpr int kCobTurnOn = -2;
    std::atomic<bool> dancing_{false};
    std::array<std::atomic<int>, uta::kCobLightCount> cob_pending_{-1, -1, -1};
    TaskHandle_t cob_task_ = nullptr;
    std::array<i2c_master_bus_handle_t, 2> oled_buses_{};
    std::array<std::atomic<int>, 2> servo_pending_{-1, -1};
    static constexpr int kNoExtraServoCommand = 1000;
    std::array<std::atomic<int>, uta::kExtraServoCount> extra_servo_pending_{
        kNoExtraServoCommand, kNoExtraServoCommand, kNoExtraServoCommand, kNoExtraServoCommand};
    std::atomic<uta::ReplyGesture> pending_gesture_{uta::ReplyGesture::None};
    uta::MediaPlayer media_;
    UtaBootButton boot_;
    std::unique_ptr<UtaDisplay> display_;
    std::unique_ptr<Display> compact_display_;
    lv_obj_t* secondary_label_ = nullptr;
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

    static esp_err_t InitLcdBus(spi_host_device_t host, int data, int clock, int miso) {
        spi_bus_config_t bus{};
        bus.mosi_io_num = data;
        bus.miso_io_num = miso;
        bus.sclk_io_num = clock;
        bus.quadwp_io_num = GPIO_NUM_NC;
        bus.quadhd_io_num = GPIO_NUM_NC;
        bus.max_transfer_sz = 240 * 320 * sizeof(uint16_t);
        return spi_bus_initialize(host, &bus, SPI_DMA_CH_AUTO);
    }

    static esp_err_t CreateLcd(spi_host_device_t host, uta::LcdDriver driver, int cs, int dc,
                               int reset, esp_lcd_panel_io_handle_t& io,
                               esp_lcd_panel_handle_t& panel) {
        esp_lcd_panel_io_spi_config_t config{};
        config.cs_gpio_num = static_cast<gpio_num_t>(cs);
        config.dc_gpio_num = static_cast<gpio_num_t>(dc);
        config.pclk_hz = driver == uta::LcdDriver::St7735 ? 20 * 1000 * 1000 : 40 * 1000 * 1000;
        config.trans_queue_depth = 10;
        config.lcd_cmd_bits = 8;
        config.lcd_param_bits = 8;
        esp_err_t result = esp_lcd_new_panel_io_spi(host, &config, &io);
        if (result != ESP_OK)
            return result;
        esp_lcd_panel_dev_config_t device{};
        device.reset_gpio_num = static_cast<gpio_num_t>(reset);
        device.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        device.bits_per_pixel = 16;
        switch (driver) {
        case uta::LcdDriver::St7735:
            device.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
            result = esp_lcd_new_panel_st7735(io, &device, &panel);
            break;
        case uta::LcdDriver::St7789:
            result = esp_lcd_new_panel_st7789(io, &device, &panel);
            break;
        case uta::LcdDriver::Ili9341:
            result = esp_lcd_new_panel_ili9341(io, &device, &panel);
            break;
        case uta::LcdDriver::Gc9a01:
            result = esp_lcd_new_panel_gc9a01(io, &device, &panel);
            break;
        default:
            result = ESP_ERR_NOT_SUPPORTED;
            break;
        }
        if (result == ESP_OK)
            result = esp_lcd_panel_reset(panel);
        if (result == ESP_OK)
            result = esp_lcd_panel_init(panel);
        if (result == ESP_OK)
            result = esp_lcd_panel_invert_color(panel, driver != uta::LcdDriver::Ili9341);
        if (result == ESP_OK)
            result = esp_lcd_panel_disp_on_off(panel, true);
        return result;
    }

    static esp_err_t CreateOled(const uta::LcdSettings& lcd, int sda, int scl, int port,
                                i2c_master_bus_handle_t& bus, esp_lcd_panel_io_handle_t& io,
                                esp_lcd_panel_handle_t& panel) {
        i2c_master_bus_config_t bus_config{};
        bus_config.i2c_port = port;
        bus_config.sda_io_num = static_cast<gpio_num_t>(sda);
        bus_config.scl_io_num = static_cast<gpio_num_t>(scl);
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;
        esp_err_t result = i2c_new_master_bus(&bus_config, &bus);
        if (result != ESP_OK)
            return result;
        esp_lcd_panel_io_i2c_config_t io_config{};
        io_config.dev_addr = 0x3c;
        io_config.scl_speed_hz = 400000;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        result = esp_lcd_new_panel_io_i2c(bus, &io_config, &io);
        if (result != ESP_OK)
            return result;
        esp_lcd_panel_ssd1306_config_t geometry{.height = static_cast<uint8_t>(lcd.height)};
        esp_lcd_panel_dev_config_t device{};
        device.reset_gpio_num = GPIO_NUM_NC;
        device.bits_per_pixel = 1;
        device.vendor_config = &geometry;
        result = lcd.driver == uta::LcdDriver::Sh1106
                     ? esp_lcd_new_panel_sh1106(io, &device, &panel)
                     : esp_lcd_new_panel_ssd1306(io, &device, &panel);
        if (result == ESP_OK)
            result = esp_lcd_panel_reset(panel);
        if (result == ESP_OK)
            result = esp_lcd_panel_init(panel);
        if (result == ESP_OK)
            result = esp_lcd_panel_disp_on_off(panel, true);
        return result;
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        const auto& lcd = active_.primary_lcd;
        if (lcd.driver == uta::LcdDriver::Off) {
            compact_display_ = std::make_unique<NoDisplay>();
            if (active_.secondary_lcd.driver != uta::LcdDriver::Off) {
                const lvgl_port_cfg_t config = ESP_LVGL_PORT_INIT_CONFIG();
                if (lvgl_port_init(&config) == ESP_OK)
                    InitializeSecondaryDisplay();
            }
            return;
        }
        if (uta::IsOled(lcd.driver)) {
            i2c_master_bus_handle_t bus = nullptr;
            if (CreateOled(lcd, Pin(uta::kLcdMosi), Pin(uta::kLcdClock), 0, bus, io, panel) != ESP_OK) {
                ESP_LOGE(kTag, "Không khởi tạo được màn hình chính I²C");
                if (panel)
                    esp_lcd_panel_del(panel);
                if (io)
                    esp_lcd_panel_io_del(io);
                if (bus)
                    i2c_del_master_bus(bus);
                compact_display_ = std::make_unique<NoDisplay>();
                return;
            }
            oled_buses_[0] = bus;
            compact_display_ = std::make_unique<OledDisplay>(io, panel, lcd.width, lcd.height,
                                                             false, false);
        } else {
            ESP_ERROR_CHECK(InitLcdBus(SPI3_HOST, Pin(uta::kLcdMosi), Pin(uta::kLcdClock),
                                       active_.touch_enabled ? Pin(uta::kTouchMiso) : GPIO_NUM_NC));
            ESP_ERROR_CHECK(CreateLcd(SPI3_HOST, lcd.driver, Pin(uta::kLcdCs), Pin(uta::kLcdDc),
                                      Pin(uta::kLcdReset), io, panel));
            if (lcd.driver == uta::LcdDriver::St7735) {
                compact_display_ = std::make_unique<SpiLcdDisplay>(
                    io, panel, lcd.width, lcd.height, 0, 0, false, false, false);
            } else {
                display_ = std::make_unique<UtaDisplay>(
                    io, panel, lcd.width, lcd.height, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
                display_->RestoreRotation();
            }
        }
        if (active_.secondary_lcd.driver != uta::LcdDriver::Off)
            InitializeSecondaryDisplay();
    }

    void InitializeSecondaryDisplay() {
        const auto& lcd = active_.secondary_lcd;
        const auto& pins = lcd.pins;
        const bool oled = uta::IsOled(lcd.driver);
        const bool shared = !oled && pins[uta::kLcdDataPin] < 0;
        const auto host = shared ? SPI3_HOST : SPI2_HOST;
        if (!oled && !shared && InitLcdBus(host, pins[uta::kLcdDataPin], pins[uta::kLcdClockPin], -1) != ESP_OK) {
            ESP_LOGW(kTag, "Không mở được bus SPI màn hình phụ");
            return;
        }
        esp_lcd_panel_io_handle_t io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        i2c_master_bus_handle_t bus = nullptr;
        const auto result = oled
                                ? CreateOled(lcd, pins[uta::kLcdDataPin], pins[uta::kLcdClockPin],
                                             uta::IsOled(active_.primary_lcd.driver) ? 1 : 0,
                                             bus, io, panel)
                                : CreateLcd(host, lcd.driver, pins[uta::kLcdSelectPin],
                                            pins[uta::kLcdCommandPin], pins[uta::kLcdResetPin], io, panel);
        if (result == ESP_OK && lvgl_port_lock(1000)) {
            auto* primary = lv_display_get_default();
            lvgl_port_display_cfg_t config{};
            config.io_handle = io;
            config.panel_handle = panel;
            config.buffer_size = lcd.width * (oled ? lcd.height : 20);
            config.hres = lcd.width;
            config.vres = lcd.height;
            config.monochrome = oled;
            config.color_format = LV_COLOR_FORMAT_RGB565;
            config.flags.buff_dma = true;
            config.flags.swap_bytes = !oled;
            auto* secondary = lvgl_port_add_disp(&config);
            if (secondary) {
                secondary_label_ = lv_label_create(lv_display_get_screen_active(secondary));
                lv_label_set_text(secondary_label_, "UTA-robot");
                lv_obj_center(secondary_label_);
                if (primary)
                    lv_display_set_default(primary);
            }
            lvgl_port_unlock();
            if (secondary) {
                oled_buses_[1] = bus;
                const int backlight = pins[uta::kLcdLightPin];
                if (!oled && backlight >= 0) {
                    gpio_set_direction(static_cast<gpio_num_t>(backlight), GPIO_MODE_OUTPUT);
                    gpio_set_level(static_cast<gpio_num_t>(backlight), 1);
                }
                return;
            }
        }
        ESP_LOGW(kTag, "Không khởi tạo được màn hình phụ");
        if (panel)
            esp_lcd_panel_del(panel);
        if (io)
            esp_lcd_panel_io_del(io);
        if (bus)
            i2c_del_master_bus(bus);
        if (!oled && !shared)
            spi_bus_free(host);
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
            command.dance = direction == "nhay";
        }
        return robot_commands_ && xQueueOverwrite(robot_commands_, &command) == pdPASS;
    }

    bool QueueServo(int axis, int angle) {
        if (!control_task_ || axis < 0 || axis > 1 || !active_.servos_enabled[axis] ||
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
            [this, value]() {
                if (auto* backlight = GetBacklight())
                    backlight->SetBrightness(std::clamp(value, 5, 100));
            });
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
        if (uta::UsesGpio(active_, BOOT_BUTTON_GPIO))
            return;
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
                config.primary_lcd = {uta::LcdDriver::St7789};
                config.secondary_lcd = {};
                config.touch_enabled = false;
                config.robot_enabled = false;
                config.motor_driver = 0;
                config.servos_enabled = {};
                config.extra_servo_pins.fill(-1);
                config.range_pins.fill(-1);
                config.cob_pins.fill(-1);
                config.rgb_common_anode = false;
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
        const auto network = GetWifiNetworkInfo();
        const auto& ip = network.config_mode ? network.ap_ip : network.ip;
        if (ip.empty()) {
            GetDisplay()->ShowNotification("Chưa nhận được địa chỉ IP; đợi kết nối Wi-Fi", 5000);
            return;
        }
        const auto address = "http://" + ip + ":" + std::to_string(UTA_HTTP_PORT);
        const std::string message = address + "\nMở trực tiếp, không cần đăng nhập";
        Application::GetInstance().Schedule([this, message]() {
            GetDisplay()->SetChatMessage("system", message.c_str());
            GetDisplay()->ShowNotification("Trang điều khiển UTA-robot", 10000);
        });
    }

    bool QueueCob(size_t light, int value) {
        if (!cob_task_ || restart_at_.load() || !cob_.Available(light))
            return false;
        cob_pending_[light].store(value);
        xTaskNotifyGive(cob_task_);
        return true;
    }

    static unsigned CobRainbow(int64_t now) {
        constexpr unsigned kPeak = 96;
        const unsigned phase = (now / 10000) % (3 * kPeak);
        const unsigned rise = phase % kPeak;
        const unsigned fall = kPeak - rise;
        if (phase < kPeak)
            return (fall << 16) | (rise << 8);
        if (phase < 2 * kPeak)
            return (fall << 8) | rise;
        return (rise << 16) | fall;
    }

    void RunCobLights() {
        std::array<int, uta::kCobLightCount> last_on{0x404040, 64, 0x404040};
        std::array<int, uta::kCobLightCount> manual{};
        std::array<int, uta::kCobLightCount> output{};
        std::array<bool, uta::kCobLightCount> disabled{};
        std::array<bool, uta::kCobLightCount> override_effect{};
        const bool color_available = cob_.Available(uta::kRgbLight) || cob_.Available(uta::kStripLight);
        bool previous_effect = false;
        for (;;) {
            ulTaskNotifyTake(pdTRUE, color_available ? pdMS_TO_TICKS(60) : portMAX_DELAY);
            const bool effect = color_available && (dancing_.load() ||
                (media_.IsPlaying() && !media_.IsPaused() && !media_.IsVideo()));
            if (effect != previous_effect) {
                override_effect.fill(false);
                previous_effect = effect;
            }
            const int rainbow = effect ? CobRainbow(esp_timer_get_time()) : 0;
            for (size_t light = 0; light < cob_pending_.size(); ++light) {
                const int value = cob_pending_[light].exchange(kCobNoCommand);
                if (value != kCobNoCommand) {
                    manual[light] = value == kCobTurnOn ? last_on[light] : value;
                    disabled[light] = manual[light] == 0;
                    if (!disabled[light])
                        last_on[light] = manual[light];
                    override_effect[light] = effect;
                }
                const bool animate = effect && light != uta::kEdisonLight &&
                                     !disabled[light] && !override_effect[light];
                const int target = animate ? rainbow : manual[light];
                if (cob_.Available(light) && target != output[light]) {
                    cob_.Apply(light, target);
                    output[light] = target;
                }
            }
        }
    }

    void RegisterCobTools(McpServer& mcp) {
        struct LightTools {
            uta::CobLight light;
            const char* power;
            const char* description;
            const char* color;
        };
        constexpr LightTools tools[] = {
            {uta::kRgbLight, "self.cob.rgb.set_power", "Bật hoặc tắt đèn RGB", "self.cob.rgb.set_color"},
            {uta::kEdisonLight, "self.cob.edison.set_power", "Bật hoặc tắt đèn Edison", nullptr},
            {uta::kStripLight, "self.cob.ws2812b.set_power", "Bật hoặc tắt dải WS2812B 8 LED",
             "self.cob.ws2812b.set_color"},
        };
        if (!cob_task_)
            return;
        for (const auto& tool : tools) {
            if (!cob_.Available(tool.light))
                continue;
            const auto light = tool.light;
            mcp.AddTool(tool.power, tool.description,
                        PropertyList({Property("on", kPropertyTypeBoolean)}),
                        [this, light](const PropertyList& args) -> ReturnValue {
                            return QueueCob(light, args["on"].value<bool>() ? kCobTurnOn : 0);
                        });
            if (tool.color) {
                mcp.AddTool(tool.color, "Đặt màu đèn bằng ba giá trị RGB từ 0 đến 255",
                            PropertyList({Property("red", kPropertyTypeInteger, 0, 255),
                                          Property("green", kPropertyTypeInteger, 0, 255),
                                          Property("blue", kPropertyTypeInteger, 0, 255)}),
                            [this, light](const PropertyList& args) -> ReturnValue {
                                const int rgb = (args["red"].value<int>() << 16) |
                                                (args["green"].value<int>() << 8) |
                                                args["blue"].value<int>();
                                return QueueCob(light, rgb);
                            });
            }
        }
        if (cob_.Available(uta::kEdisonLight)) {
            mcp.AddTool("self.cob.edison.set_brightness", "Chỉnh độ sáng Edison từ 0 đến 100 phần trăm",
                        PropertyList({Property("brightness", kPropertyTypeInteger, 0, 100)}),
                        [this](const PropertyList& args) -> ReturnValue {
                            return QueueCob(uta::kEdisonLight,
                                            (args["brightness"].value<int>() * 255 + 50) / 100);
                        });
        }
        mcp.AddTool("self.cob.set_all_power", "Bật hoặc tắt tất cả đèn COB đã cấu hình",
                    PropertyList({Property("on", kPropertyTypeBoolean)}),
                    [this](const PropertyList& args) -> ReturnValue {
                        bool accepted = false;
                        const int value = args["on"].value<bool>() ? kCobTurnOn : 0;
                        for (size_t light = 0; light < uta::kCobLightCount; ++light)
                            accepted = QueueCob(light, value) || accepted;
                        return accepted;
                    });
    }

    void RunControls() {
        int64_t stop_at = 0;
        std::array<int64_t, uta::kWheelServoCount> wheel_stop_at{};
        int64_t spectrum_at = 0;
        int64_t gesture_at = 0;
        size_t gesture_axis = 0;
        size_t gesture_step = 0;
        std::array<int, 2> servo_target = active_.servo_home;
        constexpr std::array gesture_offsets{15, -15, 0};
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
                dancing_.store(false);
                for (size_t axis = 0; axis < wheel_stop_at.size(); ++axis) {
                    extra_servo_pending_[axis].store(kNoExtraServoCommand);
                    motion_.ExtraServo(axis, 0);
                    wheel_stop_at[axis] = 0;
                }
            }
            const int64_t servo_now = esp_timer_get_time();
            for (size_t axis = 0; axis < extra_servo_pending_.size(); ++axis) {
                const int value = extra_servo_pending_[axis].exchange(kNoExtraServoCommand);
                if (value != kNoExtraServoCommand) {
                    motion_.ExtraServo(axis, value);
                    if (axis < wheel_stop_at.size())
                        wheel_stop_at[axis] = value == 0 ? 0 : servo_now + 400000;
                }
            }
            for (size_t axis = 0; axis < wheel_stop_at.size(); ++axis) {
                if (wheel_stop_at[axis] && servo_now >= wheel_stop_at[axis]) {
                    motion_.ExtraServo(axis, 0);
                    wheel_stop_at[axis] = 0;
                }
            }
            bool manual_servo = false;
            for (size_t axis = 0; axis < 2; ++axis) {
                const int angle = servo_pending_[axis].exchange(-1);
                if (angle >= 0) {
                    manual_servo = true;
                    if (gesture_at && axis == gesture_axis)
                        gesture_at = 0;
                    servo_target[axis] = angle;
                    motion_.Servo(axis, angle);
                }
            }
            RobotCommand command;
            if (robot_commands_ && xQueueReceive(robot_commands_, &command, 0) == pdTRUE) {
                WriteRobot(command.text);
                if (command.duration_ms > 0) {
                    stop_at = esp_timer_get_time() + command.duration_ms * 1000LL;
                    dancing_.store(command.dance);
                }
            }
            const int64_t now = esp_timer_get_time();
            const auto gesture = pending_gesture_.exchange(uta::ReplyGesture::None);
            if (gesture != uta::ReplyGesture::None && !gesture_at && !manual_servo) {
                gesture_axis = gesture == uta::ReplyGesture::Nod ? 0 : 1;
                gesture_step = 0;
                gesture_at = now;
            }
            if (gesture_at && now >= gesture_at) {
                const int target = servo_target[gesture_axis] + gesture_offsets[gesture_step++];
                motion_.Servo(gesture_axis, target);
                gesture_at = gesture_step < gesture_offsets.size() ? now + 220000 : 0;
            }
            if (now - spectrum_at >= 100000) {
                if (display_) {
                    display_->UpdateStandby(Application::GetInstance().GetDeviceState(),
                                            media_.IsPlaying() || media_.IsVideo(),
                                            WifiManager::GetInstance().IsConnected());
                    const bool visible =
                        media_.IsPlaying() && !media_.IsVideo() &&
                        Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
                    display_->UpdateSpectrum(media_.GetSpectrum(), visible);
                    if (!media_.IsVideo() ||
                        Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
                        display_->UpdateVideo(nullptr, 0, 0);
                }
                if (secondary_label_ && lvgl_port_lock(5)) {
                    const char* text = WifiManager::GetInstance().IsConnected()
                                           ? "UTA-robot\nWi-Fi connected"
                                           : "UTA-robot\nWi-Fi offline";
                    if (std::strcmp(lv_label_get_text(secondary_label_), text) != 0)
                        lv_label_set_text(secondary_label_, text);
                    lvgl_port_unlock();
                }
                spectrum_at = now;
            }
            if (stop_at && now >= stop_at) {
                WriteRobot("x0.0 y0.0");
                stop_at = 0;
                dancing_.store(false);
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
        RegisterCobTools(mcp);
        mcp.AddTool("self.screen.set_rotation",
                    "Xoay màn hình tới góc tuyệt đối 0, 90, 180 hoặc 270 độ và lưu góc.",
                    PropertyList({Property("degrees", kPropertyTypeInteger, 0, 270)}),
                    [this](const PropertyList& values) -> ReturnValue {
                        return display_ && display_->SetRotation(values["degrees"].value<int>());
                    });
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
        if (active_.servos_enabled[0] || active_.servos_enabled[1]) {
            mcp.AddTool("self.robot.set_head", "Đặt góc đầu và cổ, trong giới hạn đã lưu trên web",
                        PropertyList({Property("dau", kPropertyTypeInteger, 90, 0, 180),
                                      Property("co", kPropertyTypeInteger, 90, 0, 180)}),
                        [this](const PropertyList& values) -> ReturnValue {
                            const int head = values["dau"].value<int>();
                            const int neck = values["co"].value<int>();
                            if ((active_.servos_enabled[0] &&
                                 (head < active_.servo_min[0] || head > active_.servo_max[0])) ||
                                (active_.servos_enabled[1] &&
                                 (neck < active_.servo_min[1] || neck > active_.servo_max[1])))
                                return false;
                            const bool head_ok = !active_.servos_enabled[0] || QueueServo(0, head);
                            const bool neck_ok = !active_.servos_enabled[1] || QueueServo(1, neck);
                            return head_ok && neck_ok;
                        });
        }
        uta::RegisterMediaTools(media_, settings_);
    }
};

DECLARE_BOARD(UtaRobot);
