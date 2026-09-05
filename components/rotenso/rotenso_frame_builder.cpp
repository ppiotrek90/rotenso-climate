#include "rotenso_frame_builder.h"

#include <cmath>
#include <unordered_map>

#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.climate";

static const std::unordered_map<std::string, uint8_t> VERTICAL_VANE_BYTES = {
    {"Top", 0x01},
    {"Upper", 0x02},
    {"Mid", 0x03},
    {"Lower", 0x04},
    {"Bottom", 0x05},
    {"Move Full", 0x0D},
    {"Move Upper", 0x15},
    {"Move Lower", 0x1D},
};

uint8_t RotensoFrameBuilder::vertical_vane_position_to_byte(
    const std::string &position) {
  auto it = VERTICAL_VANE_BYTES.find(position);
  return it != VERTICAL_VANE_BYTES.end() ? it->second : 0x00;
}

bool RotensoFrameBuilder::vertical_vane_position_needs_move_bit(
    const std::string &position) {
  return position == "Move Full" ||
         position == "Move Upper" ||
         position == "Move Lower";
}

RotensoFrameBuilder::RotensoFrameBuilder() {
  frame_ = {
      0xBB,  // 0
      0x00,  // 1
      0x01,  // 2
      0x03,  // 3
      0x21,  // 4
      0x00,  // 5
      0x00,  // 6
      0x60,  // 7 - Power OFF by default
      0x00,  // 8 - Preset + Mode
      0x18,  // 9 - Temperature placeholder
      0x00,  // 10 - Fan / Move bits
      0x00,  // 11
      0x00,  // 12
      0x00,  // 13
      0x00,  // 14
      0x00,  // 15
      0x00,  // 16
      0x00,  // 17
      0x00,  // 18
      0x00,  // 19
      0x00,  // 20
      0x00,  // 21
      0x00,  // 22
      0x00,  // 23
      0x00,  // 24
      0x00,  // 25
      0x00,  // 26
      0x00,  // 27
      0x00,  // 28
      0x00,  // 29
      0x00,  // 30
      0x00,  // 31
      0x00,  // 32 - Vertical vane
      0x00,  // 33
      0x00,  // 34
      0x00,  // 35
      0x00,  // 36
      0x00,  // 37
      0x00   // 38 - checksum
  };
}

void RotensoFrameBuilder::from_climate_state(
    const climate::Climate *climate,
    const climate::ClimateCall &call) {
  bool is_on =
      climate->mode != climate::CLIMATE_MODE_OFF;

  float target_temp =
      climate->target_temperature;

  // Power.
  frame_[7] = encode_power(is_on);

  // Mode + preset.
  climate::ClimatePreset preset =
      climate->preset.has_value()
          ? *climate->preset
          : climate::CLIMATE_PRESET_NONE;

  frame_[8] =
      encode_mode_preset(climate->mode, preset);

  // Eco preset - byte[7] bit 0x80. CONFIRMED against real hardware
  // (captured from the genuine Tuya module's own traffic).
  if (preset == climate::CLIMATE_PRESET_ECO) {
    frame_[7] |= 0x80;
  }

  // Sleep preset
  frame_[19] =
      (preset == climate::CLIMATE_PRESET_SLEEP)
          ? 0x01
          : 0x00;

  // Fan.
  climate::ClimateFanMode fan_mode =
      call.get_fan_mode().value_or(
          climate->fan_mode.has_value()
              ? *climate->fan_mode
              : climate::CLIMATE_FAN_AUTO);

  set_fan_speed(fan_mode);

  // Target temperature.
  encode_temperature(target_temp);

}

void RotensoFrameBuilder::set_anti_mildew(bool enabled) {
  // CONFIRMED working against real hardware, captured from the genuine
  // Tuya module's own traffic: byte[8] bit 0x20, independent of the
  // preset bits already occupying that nibble (None/Comfort/Boost/Eco).
  if (enabled) {
    frame_[8] |= 0x20;
  } else {
    frame_[8] &= ~0x20;
  }

  ESP_LOGD(
      TAG,
      "Anti-mildew: %s -> byte[8]=0x%02X",
      enabled ? "On" : "Off",
      frame_[8]);
}

void RotensoFrameBuilder::set_vertical_vane_position(
    const std::string &position) {
  const uint8_t vane_byte =
      vertical_vane_position_to_byte(position);

  frame_[32] = vane_byte;

  if (vertical_vane_position_needs_move_bit(position)) {
    // IMPORTANT:
    // byte[10] already contains the fan setting.
    // OR 0x38 instead of replacing byte[10].
    frame_[10] |= 0x38;
  }

  ESP_LOGD(
      TAG,
      "Vane position: %s -> byte[32]=0x%02X%s",
      position.c_str(),
      frame_[32],
      vertical_vane_position_needs_move_bit(position)
          ? ", byte[10] |= 0x38"
          : "");
}

static const std::unordered_map<std::string, uint8_t> HORIZONTAL_VANE_BYTES = {
    {"Left", 0x01},
    {"Mid-left", 0x02},
    {"Mid", 0x03},
    {"Mid-right", 0x04},
    {"Right", 0x05},
    {"Move Full", 0x08},
    {"Move Left", 0x10},
    {"Move Mid", 0x18},
    {"Move Right", 0x20},
};

uint8_t RotensoFrameBuilder::horizontal_vane_position_to_byte(
    const std::string &position) {
  // CONFIRMED against real hardware: byte 33, bits 0-2 = fix, bits 3-5 = mv.
  auto it = HORIZONTAL_VANE_BYTES.find(position);
  return it != HORIZONTAL_VANE_BYTES.end() ? it->second : 0x00;
}

bool RotensoFrameBuilder::horizontal_vane_position_needs_move_bit(
    const std::string &position) {
  return position == "Move Full" ||
         position == "Move Left" ||
         position == "Move Mid" ||
         position == "Move Right";
}

void RotensoFrameBuilder::set_horizontal_vane_position(
    const std::string &position) {
  frame_[33] = horizontal_vane_position_to_byte(position);

  // CONFIRMED against real hardware: byte[11] bit 0x08 is the horizontal
  // movement enable bit. Must be CLEARED for a fixed position and SET for
  // the "Move" sub-modes. byte[11] also carries the whole/half-degree
  // temperature bit set earlier by encode_temperature() - only touch
  // bit 0x08 here.
  if (horizontal_vane_position_needs_move_bit(position)) {
    frame_[11] |= 0x08;
  } else {
    frame_[11] &= ~0x08;
  }

  ESP_LOGD(
      TAG,
      "Horizontal vane position: %s -> byte[33]=0x%02X, byte[11]=0x%02X",
      position.c_str(),
      frame_[33],
      frame_[11]);
}

void RotensoFrameBuilder::set_raw_byte(
    size_t index,
    uint8_t value) {
  if (index < FRAME_LENGTH - 1) {
    frame_[index] = value;
  }
}

void RotensoFrameBuilder::or_raw_byte(
    size_t index,
    uint8_t bits) {
  if (index < FRAME_LENGTH - 1) {
    frame_[index] |= bits;
  }
}

uint8_t RotensoFrameBuilder::encode_power(
    bool power) {
  // CONFIRMED against real hardware: byte[7] bit 0x04 is the actual power
  // bit. The old 0x60/0x64 constants baked in bits 0x20 (buzzer) and 0x40
  // (display) as always-on, which we now control independently instead.
  return power ? 0x04 : 0x00;
}

void RotensoFrameBuilder::set_buzzer(bool enabled) {
  // CONFIRMED against real hardware: byte[7] bit 0x20.
  if (enabled) {
    frame_[7] |= 0x20;
  } else {
    frame_[7] &= ~0x20;
  }
}

void RotensoFrameBuilder::set_display(bool enabled) {
  // CONFIRMED against real hardware: byte[7] bit 0x40.
  if (enabled) {
    frame_[7] |= 0x40;
  } else {
    frame_[7] &= ~0x40;
  }
}

uint8_t RotensoFrameBuilder::encode_mode_preset(
    climate::ClimateMode mode,
    climate::ClimatePreset preset) {
  uint8_t preset_val = 0x0;

  switch (preset) {
    case climate::CLIMATE_PRESET_NONE:
      preset_val = 0x0;
      break;

    case climate::CLIMATE_PRESET_ECO:
      // NOTE: this preset_val=0x8 (byte[8] upper nibble) was NEVER
      // confirmed against real hardware - it was inherited, unverified,
      // from the original repo. A real capture now confirms ECO is
      // actually byte[7] bit 0x80 (handled separately in
      // from_climate_state()), so this case intentionally does nothing
      // to byte[8] anymore.
      preset_val = 0x0;
      break;

    case climate::CLIMATE_PRESET_BOOST:
      // Confirmed via real Tuya capture as "Turbo Fan" - same bit, just
      // using the standard preset name/mechanism rather than a custom one.
      preset_val = 0x4;
      break;

    case climate::CLIMATE_PRESET_COMFORT:
      preset_val = 0x1;
      break;

    default:
      preset_val = 0x0;
      break;
  }

  uint8_t mode_val = 0x8;

  switch (mode) {
    case climate::CLIMATE_MODE_COOL:
      mode_val = 0x3;
      break;

    case climate::CLIMATE_MODE_HEAT:
      mode_val = 0x1;
      break;

    case climate::CLIMATE_MODE_DRY:
      mode_val = 0x2;
      break;

    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_val = 0x7;
      break;

    case climate::CLIMATE_MODE_AUTO:
      mode_val = 0x8;
      break;

    default:
      mode_val = 0x8;
      break;
  }

  return (preset_val << 4) | mode_val;
}

void RotensoFrameBuilder::encode_temperature(
    float temperature) {
  int temp_int =
      static_cast<int>(temperature);

  if (temp_int < 16)
    temp_int = 16;

  if (temp_int > 31)
    temp_int = 31;

  // Byte[9]:
  // 0x5X where X = (0xF + 16 - T) & 0x0F
  uint8_t encoded_low_nibble =
      static_cast<uint8_t>(
          (0xF + 16 - temp_int) & 0x0F);

  frame_[9] =
      0x50 | encoded_low_nibble;

  // Byte[11]:
  // 0x0A for .5
  // 0x08 otherwise
  float decimal =
      temperature - static_cast<float>(temp_int);

  frame_[11] =
      (std::abs(decimal - 0.5f) < 0.01f)
          ? 0x0A
          : 0x08;
}

void RotensoFrameBuilder::set_fan_speed(
    climate::ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_AUTO:
      frame_[10] = 0x00;
      break;

    case climate::CLIMATE_FAN_LOW:
      frame_[10] = 0x02;
      break;

    case climate::CLIMATE_FAN_MEDIUM:
      frame_[10] = 0x03;
      break;

    case climate::CLIMATE_FAN_HIGH:
      frame_[10] = 0x05;
      break;

    case climate::CLIMATE_FAN_QUIET:
      frame_[8] |= 0x80;
      frame_[10] = 0x01;
      break;

    default:
      frame_[10] = 0x00;
      break;
  }

  ESP_LOGD(
      TAG,
      "Fan mode set in ESPHome: %s -> Frame byte[10] = 0x%02X",
      climate::climate_fan_mode_to_string(fan_mode),
      frame_[10]);
}

void RotensoFrameBuilder::update_checksum() {
  uint8_t checksum = 0x00;

  for (size_t i = 0; i < FRAME_LENGTH - 1; ++i) {
    checksum ^= frame_[i];
  }

  frame_[FRAME_LENGTH - 1] =
      checksum;
}

std::array<uint8_t, RotensoFrameBuilder::FRAME_LENGTH>
RotensoFrameBuilder::build_frame() {
  this->update_checksum();

  return frame_;
}

}  // namespace rotenso
}  // namespace esphome