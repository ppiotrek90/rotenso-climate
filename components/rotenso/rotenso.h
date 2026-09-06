#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace rotenso {

class RotensoClimate;

// Vertical vane position select.
class RotensoVerticalVaneSelect : public select::Select {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;

  RotensoClimate *parent_{nullptr};
};

// Horizontal vane position select.
class RotensoHorizontalVaneSelect : public select::Select {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;

  RotensoClimate *parent_{nullptr};
};

// Anti-mildew control switch.
class RotensoAntiMildewSwitch : public switch_::Switch {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  RotensoClimate *parent_{nullptr};
};

// Health / air purification control switch.
class RotensoHealthSwitch : public switch_::Switch {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  RotensoClimate *parent_{nullptr};
};

// Buzzer control switch.
class RotensoBuzzerSwitch : public switch_::Switch {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  RotensoClimate *parent_{nullptr};
};

// Indoor unit display control switch.
class RotensoDisplaySwitch : public switch_::Switch {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  RotensoClimate *parent_{nullptr};
};

class RotensoClimate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  void set_vertical_vane_select(select::Select *select) {
    this->vertical_vane_select_ = select;
  }

  void set_horizontal_vane_select(select::Select *select) {
    this->horizontal_vane_select_ = select;
  }

  void control_vertical_vane(size_t index);
  void control_horizontal_vane(size_t index);

  void set_coil_temperature_sensor(sensor::Sensor *s) {
    this->coil_temperature_sensor_ = s;
  }

  void set_room_temperature_sensor(sensor::Sensor *s) {
    this->room_temperature_sensor_ = s;
  }

  void set_error_binary_sensor(binary_sensor::BinarySensor *s) {
    this->error_binary_sensor_ = s;
  }

  void set_anti_mildew_switch(switch_::Switch *s) {
    this->anti_mildew_switch_ = s;
  }

  void set_health_switch(switch_::Switch *s) {
    this->health_switch_ = s;
  }

  void set_buzzer_switch(switch_::Switch *s) {
    this->buzzer_switch_ = s;
  }

  void set_display_switch(switch_::Switch *s) {
    this->display_switch_ = s;
  }

  void control_anti_mildew(bool state);
  void control_health(bool state);
  void control_buzzer(bool state);
  void control_display(bool state);

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void send_heartbeat();
  void parse_uart_response();
  void process_heartbeat_frame_(const std::vector<uint8_t> &frame);

  void send_current_state_frame_();
  void publish_vertical_vane_state_(uint8_t raw);
  void publish_horizontal_vane_state_(uint8_t raw);
  void update_swing_mode_();
  bool pending_timed_out_(uint32_t sent_at) const;

  // Maximum time to keep an ESPHome change before accepting AC feedback.
  static constexpr uint32_t PENDING_TIMEOUT_MS = 2000;

  // Expected RX heartbeat frame size.
  static constexpr size_t HEARTBEAT_FRAME_SIZE = 61;

  // Protection against incomplete or corrupted UART data.
  static constexpr size_t UART_RX_BUFFER_MAX = 256;

  // Buffer for assembling complete UART frames.
  std::vector<uint8_t> uart_rx_buffer_;

  // Current vertical vane position.
  std::string vertical_vane_position_{"Off"};
  select::Select *vertical_vane_select_{nullptr};
  size_t last_published_vertical_vane_index_{99};

  // Current horizontal vane position.
  std::string horizontal_vane_position_{"Off"};
  select::Select *horizontal_vane_select_{nullptr};
  size_t last_published_horizontal_vane_index_{99};

  // Pending target temperature waiting for AC confirmation.
  bool pending_target_temperature_valid_{false};
  float pending_target_temperature_{0.0f};
  uint32_t pending_target_temperature_sent_at_{0};

  // Pending climate mode waiting for AC confirmation.
  bool pending_mode_valid_{false};
  climate::ClimateMode pending_mode_{climate::CLIMATE_MODE_OFF};
  uint32_t pending_mode_sent_at_{0};

  // Pending fan mode waiting for AC confirmation.
  bool pending_fan_mode_valid_{false};
  climate::ClimateFanMode pending_fan_mode_{climate::CLIMATE_FAN_AUTO};
  uint32_t pending_fan_mode_sent_at_{0};

  // Pending vertical vane position waiting for AC confirmation.
  bool pending_vertical_vane_valid_{false};
  std::string pending_vertical_vane_position_{"Off"};
  uint32_t pending_vertical_vane_sent_at_{0};

  // Pending horizontal vane position waiting for AC confirmation.
  bool pending_horizontal_vane_valid_{false};
  std::string pending_horizontal_vane_position_{"Off"};
  uint32_t pending_horizontal_vane_sent_at_{0};

  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};

  sensor::Sensor *coil_temperature_sensor_{nullptr};
  sensor::Sensor *room_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *error_binary_sensor_{nullptr};

  // Anti-mildew state is unknown until reported by the AC.
  switch_::Switch *anti_mildew_switch_{nullptr};
  bool anti_mildew_state_{false};
  bool anti_mildew_state_valid_{false};

  // Health state is unknown until reported by the AC.
  switch_::Switch *health_switch_{nullptr};
  bool health_state_{false};
  bool health_state_valid_{false};

  // Buzzer defaults to ON until the state can be confirmed.
  switch_::Switch *buzzer_switch_{nullptr};
  bool buzzer_state_{true};
  bool buzzer_state_valid_{true};

  // Display state is unknown until reported by the AC.
  switch_::Switch *display_switch_{nullptr};
  bool display_state_{false};
  bool display_state_valid_{false};
};

}  // namespace rotenso
}  // namespace esphome