#pragma once

#include <array>
#include <string>

#include "esphome/components/climate/climate.h"

namespace esphome {
namespace rotenso {

class RotensoFrameBuilder {
 public:
  static constexpr size_t FRAME_LENGTH = 39;

  RotensoFrameBuilder();

  void from_climate_state(
      const climate::Climate *climate,
      const climate::ClimateCall &call);

  // Apply vertical vane position.
  void set_vane_position(const std::string &position);

  // Temporary test helpers.
  void set_raw_byte(size_t index, uint8_t value);

  void or_raw_byte(size_t index, uint8_t bits);

  std::array<uint8_t, FRAME_LENGTH> build_frame();

 private:
  std::array<uint8_t, FRAME_LENGTH> frame_;

  void update_checksum();

  uint8_t encode_power(bool power);

  uint8_t encode_mode_preset(
      climate::ClimateMode mode,
      climate::ClimatePreset preset);

  void encode_temperature(float temp_c);

  void set_fan_speed(climate::ClimateFanMode fan_mode);

  static uint8_t vane_position_to_byte(
      const std::string &position);

  static bool vane_position_needs_move_bit(
      const std::string &position);
};

}  // namespace rotenso
}  // namespace esphome