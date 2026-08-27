#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace rotenso {

class RotensoClimate : public climate::Climate, public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  // TEMPORARY test helper: builds a normal SET frame from the current climate
  // state, overrides one byte with a chosen value, and sends it. Used to
  // experimentally find which byte controls a given feature (e.g. sleep,
  // quiet fan) on the write side. Safe to call from a template button.
  void send_test_frame(uint8_t byte_index, uint8_t value);

  // Same as send_test_frame, but ORs the bits into the byte instead of
  // replacing it - needed when the byte already carries something else
  // (e.g. byte[10] also carries fan speed) that we don't want to clobber.
  void send_test_frame_or(uint8_t byte_index, uint8_t bits);

  // Same as send_test_frame_or, but ORs bits into TWO bytes in the SAME
  // frame at once - needed when a feature requires two bits set together
  // (e.g. quiet fan needed byte[8] and byte[10] together; some features
  // may need e.g. byte[10] and byte[32] together).
  void send_test_frame_or2(uint8_t byte_index1, uint8_t bits1, uint8_t byte_index2, uint8_t bits2);

  // Diagnostic sensors, set from YAML via the rotenso sensor.py platform.
  void set_coil_temperature_sensor(sensor::Sensor *s) { this->coil_temperature_sensor_ = s; }
  void set_room_temperature_sensor(sensor::Sensor *s) { this->room_temperature_sensor_ = s; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *s) { this->error_binary_sensor_ = s; }
  void set_anti_mildew_binary_sensor(binary_sensor::BinarySensor *s) { this->anti_mildew_binary_sensor_ = s; }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void update() override;

  void send_heartbeat();
  void parse_uart_response();

  uint32_t last_heartbeat_{0};
  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};

  sensor::Sensor *coil_temperature_sensor_{nullptr};
  sensor::Sensor *room_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *anti_mildew_binary_sensor_{nullptr};
};

}  // namespace rotenso
}  // namespace esphome