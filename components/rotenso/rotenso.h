#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace rotenso {

class RotensoClimate;

class RotensoVerticalVaneSelect : public select::Select {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;

  RotensoClimate *parent_{nullptr};
};

class RotensoHorizontalVaneSelect : public select::Select {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;

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

  // TEMPORARY
  void send_test_frame(uint8_t byte_index, uint8_t value);
  void send_test_frame2(uint8_t byte_index1, uint8_t value1,
                        uint8_t byte_index2, uint8_t value2);
  void send_test_frame_or(uint8_t byte_index, uint8_t bits);
  void send_test_frame_or2(uint8_t byte_index1, uint8_t bits1,
                           uint8_t byte_index2, uint8_t bits2);
  // Sensors
  void set_coil_temperature_sensor(sensor::Sensor *s) { this->coil_temperature_sensor_ = s; }
  void set_room_temperature_sensor(sensor::Sensor *s) { this->room_temperature_sensor_ = s; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *s) { this->error_binary_sensor_ = s; }
  void set_anti_mildew_binary_sensor(binary_sensor::BinarySensor *s) {
    this->anti_mildew_binary_sensor_ = s;
  }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void send_heartbeat();
  void parse_uart_response();

  void send_current_state_frame_();
  void publish_vertical_vane_state_(uint8_t raw);
  void publish_horizontal_vane_state_(uint8_t raw);
  void update_swing_mode_();

  std::string vertical_vane_position_{"Off"};
  select::Select *vertical_vane_select_{nullptr};
  size_t last_published_vertical_vane_index_{99};
  std::string horizontal_vane_position_{"Off"};
  select::Select *horizontal_vane_select_{nullptr};
  size_t last_published_horizontal_vane_index_{99};
  // Last vane command timestamp; ignore stale heartbeat vane updates briefly.
  uint32_t vane_command_sent_at_{0};

  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};

  sensor::Sensor *coil_temperature_sensor_{nullptr};
  sensor::Sensor *room_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *anti_mildew_binary_sensor_{nullptr};

  bool has_published_coil_temperature_{false};
  float last_published_coil_temperature_{0.0f};
  bool has_published_room_temperature_{false};
  float last_published_room_temperature_{0.0f};
  bool has_published_error_state_{false};
  bool last_published_error_state_{false};
  bool has_published_anti_mildew_state_{false};
  bool last_published_anti_mildew_state_{false};
};

}  // namespace rotenso
}  // namespace esphome