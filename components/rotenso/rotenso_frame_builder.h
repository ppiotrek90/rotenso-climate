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

  // Set vertical vane position.
  void set_vertical_vane_position(const std::string &position);

  // Set horizontal vane position.
  void set_horizontal_vane_position(const std::string &position);

  // Set anti-mildew state (byte 8, bit 0x20).
  void set_anti_mildew(bool enabled);

  // Set Health state (byte 8, bit 0x10).
  void set_health(bool enabled);

  // Set buzzer state (byte 7, bit 0x20).
  void set_buzzer(bool enabled);

  // Set display state (byte 7, bit 0x40).
  void set_display(bool enabled);

  std::array<uint8_t, FRAME_LENGTH> build_frame();

 private:
  std::array<uint8_t, FRAME_LENGTH> frame_;

  // Update frame checksum.
  void update_checksum();

  // Encode power state.
  uint8_t encode_power(bool power);

  // Encode climate mode and preset.
  uint8_t encode_mode_preset(
      climate::ClimateMode mode,
      climate::ClimatePreset preset);

  // Encode target temperature.
  void encode_temperature(float temp_c);

  // Encode fan speed.
  void set_fan_speed(climate::ClimateFanMode fan_mode);

  // Convert vertical vane position to protocol byte.
  static uint8_t vertical_vane_position_to_byte(
      const std::string &position);

  // Check if vertical vane movement bit is required.
  static bool vertical_vane_position_needs_move_bit(
      const std::string &position);

  // Convert horizontal vane position to protocol byte.
  static uint8_t horizontal_vane_position_to_byte(
      const std::string &position);

  // Check if horizontal vane movement bit is required.
  static bool horizontal_vane_position_needs_move_bit(
      const std::string &position);
};

}  // namespace rotenso
}  // namespace esphome