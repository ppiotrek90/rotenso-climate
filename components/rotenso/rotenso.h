#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace rotenso {

class RotensoClimate;

// Dedicated select implementation for vertical vane control.
// ESPHome 2026.1+ uses index-based Select callbacks, so overriding
// Select::control(size_t) keeps this independent from the old callback API.
class RotensoVaneSelect : public select::Select, public Component {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;

 private:
  RotensoClimate *parent_{nullptr};
};

class RotensoClimate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  // Vertical vane select callback target.
  void control_vertical_vane(const std::string &position);
  void set_vertical_vane_select(select::Select *select) { this->vertical_vane_select_ = select; }

  // Temporary test helpers.
  void send_test_frame(uint8_t byte_index, uint8_t value);
  void send_test_frame_or(uint8_t byte_index, uint8_t bits);
  void send_test_frame_or2(uint8_t byte_index1, uint8_t bits1, uint8_t byte_index2, uint8_t bits2);

  // Diagnostic sensors, set from YAML via the rotenso sensor.py platform.
  void set_coil_temperature_sensor(sensor::Sensor *s) { this->coil_temperature_sensor_ = s; }
  void set_room_temperature_sensor(sensor::Sensor *s) { this->room_temperature_sensor_ = s; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *s) { this->error_binary_sensor_ = s; }
  void set_anti_mildew_binary_sensor(binary_sensor::BinarySensor *s) { this->anti_mildew_binary_sensor_ = s; }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void send_heartbeat();
  void parse_uart_response();

  static constexpr size_t HEARTBEAT_FRAME_LENGTH = 61;
  static constexpr size_t SECONDARY_FRAME_LENGTH = 51;
  static constexpr uint8_t FRAME_START = 0xBB;
  static constexpr uint8_t HEARTBEAT_COMMAND = 0x04;
  static constexpr uint8_t SECONDARY_COMMAND = 0x09;

  std::vector<uint8_t> rx_buffer_;
  std::string vertical_vane_position_{"Off"};

  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};

  select::Select *vertical_vane_select_{nullptr};
  sensor::Sensor *coil_temperature_sensor_{nullptr};
  sensor::Sensor *room_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *anti_mildew_binary_sensor_{nullptr};
};

}  // namespace rotenso
}  // namespace esphome
