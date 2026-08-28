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

  // Apply vertical vane position - byte 32 (position) + byte 10 bits 3-5
  // (movement enable). CONFIRMED working against real hardware.
  void set_vertical_vane_position(const std::string &position);

  // Apply horizontal vane position - byte 33 (position) + byte 11 bit 0x08
  // (movement enable). CONFIRMED working against real hardware.
  void set_horizontal_vane_position(const std::string &position);

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

  static uint8_t vertical_vane_position_to_byte(
      const std::string &position);

  static bool vertical_vane_position_needs_move_bit(
      const std::string &position);

  static uint8_t horizontal_vane_position_to_byte(
      const std::string &position);

  static bool horizontal_vane_position_needs_move_bit(
      const std::string &position);
};

}  // namespace rotenso
}  // namespace esphome
