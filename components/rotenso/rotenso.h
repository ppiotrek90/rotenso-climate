#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
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

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void update() override;

  void send_heartbeat();
  void parse_uart_response();

  uint32_t last_heartbeat_{0};
  climate::ClimatePreset preset_{climate::CLIMATE_PRESET_NONE};
};

}  // namespace rotenso
}  // namespace esphome