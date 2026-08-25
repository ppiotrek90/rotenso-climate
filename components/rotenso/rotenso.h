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

  // TEMPORARY test helper: sends byte[8]|=0x80 AND byte[10]|=0x01 together
  // in one frame, per the quiet-fan encoding found in a related TCL project.
  void send_quiet_test();

  // Diagnostic sensors, set from YAML via the rotenso sensor.py platform.
  void set_coil_temperature_sensor(sensor::Sensor *s) { this->coil_temperature_sensor_ = s; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *s) { this->error_binary_sensor_ = s; }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void update() override;

  void send_heartbeat();
  void parse_uart_response();

  uint32_t last_heartbeat_{0};
  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};

  sensor::Sensor *coil_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *error_binary_sensor_{nullptr};
};

}  // namespace rotenso
}  // namespace esphome