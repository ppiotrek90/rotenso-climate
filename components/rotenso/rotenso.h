#pragma once

#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace rotenso {

class RotensoClimate;

class RotensoVaneSelect : public select::Select, public Component {
 public:
  void set_parent(RotensoClimate *parent) { this->parent_ = parent; }
  void control(size_t index) override;

 protected:
  RotensoClimate *parent_{nullptr};
};

class RotensoClimate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  void set_vertical_vane_select(RotensoVaneSelect *s) { this->vertical_vane_select_ = s; }
  void control_vertical_vane(const std::string &position);

  void send_test_frame(uint8_t byte_index, uint8_t value);
  void send_test_frame_or(uint8_t byte_index, uint8_t bits);
  void send_test_frame_or2(uint8_t byte_index1, uint8_t bits1, uint8_t byte_index2, uint8_t bits2);

  void set_coil_temperature_sensor(sensor::Sensor *s) { this->coil_temperature_sensor_ = s; }
  void set_room_temperature_sensor(sensor::Sensor *s) { this->room_temperature_sensor_ = s; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *s) { this->error_binary_sensor_ = s; }
  void set_anti_mildew_binary_sensor(binary_sensor::BinarySensor *s) { this->anti_mildew_binary_sensor_ = s; }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void send_heartbeat();
  void parse_uart_response();
  void process_frame(const std::vector<uint8_t> &frame);

  std::string vertical_vane_position_{"Off"};
  std::vector<uint8_t> rx_buffer_;
  RotensoVaneSelect *vertical_vane_select_{nullptr};

  sensor::Sensor *coil_temperature_sensor_{nullptr};
  sensor::Sensor *room_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *anti_mildew_binary_sensor_{nullptr};
};

}  // namespace rotenso
}  // namespace esphome
