#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace rotenso
  {

    static const char *const TAG = "rotenso.climate";

    RotensoFrameBuilder::RotensoFrameBuilder()
    {
      frame_ = {0xBB, 0x00, 0x01, 0x03, 0x21, 0x00, 0x00,
                0x60, // Power OFF by default
                0x00, // Preset+Mode
                0x18, // Temperature placeholder
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x80, 0x00, 0x00, 0x00,
                0x00, 0x00};
    }

    void RotensoFrameBuilder::from_climate_state(const climate::Climate *climate, const climate::ClimateCall &call)
    {
      bool is_on = climate->mode != climate::CLIMATE_MODE_OFF;
      float target_temp = climate->target_temperature;

      frame_[7] = encode_power(is_on);

      climate::ClimatePreset preset = climate->preset.has_value() ? *climate->preset : climate::CLIMATE_PRESET_NONE;
      frame_[8] = encode_mode_preset(climate->mode, preset);

      climate::ClimateFanMode fan_mode = call.get_fan_mode().value_or(climate::CLIMATE_FAN_AUTO);
      set_fan_speed(fan_mode);

      encode_temperature(target_temp);
    }

    uint8_t RotensoFrameBuilder::encode_power(bool power)
    {
      return power ? 0x64 : 0x60;
    }

    uint8_t RotensoFrameBuilder::encode_mode_preset(climate::ClimateMode mode, climate::ClimatePreset preset)
    {
      uint8_t preset_val = 0x0;
      switch (preset)
      {
      case climate::CLIMATE_PRESET_NONE:
        preset_val = 0x0;
        break;
      case climate::CLIMATE_PRESET_ECO:
        preset_val = 0x8; // low noise
        break;
      case climate::CLIMATE_PRESET_BOOST:
        preset_val = 0x4; // turbo
        break;
      case climate::CLIMATE_PRESET_COMFORT:
        preset_val = 0x1; // health
        break;
      default:
        preset_val = 0x0;
        break;
      }

      uint8_t mode_val = 0x8; // default to auto
      switch (mode)
      {
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

    void RotensoFrameBuilder::encode_temperature(float temperature)
    {
      // Clamp temperature between 16 and 31
      int temp_int = static_cast<int>(temperature);
      if (temp_int < 16)
        temp_int = 16;
      if (temp_int > 31)
        temp_int = 31;

      // Byte 9: 0x5X where X = (0xF + 16 - T) & 0x0F
      uint8_t encoded_low_nibble = static_cast<uint8_t>((0xF + 16 - temp_int) & 0x0F);
      frame_[9] = 0x50 | encoded_low_nibble;

      // Byte 11: 0x0A if decimal is .5, else 0x08
      float decimal = temperature - static_cast<float>(temp_int);
      frame_[11] = (std::abs(decimal - 0.5f) < 0.01f) ? 0x0A : 0x08;
    }

    void RotensoFrameBuilder::set_fan_speed(climate::ClimateFanMode fan_mode)
    {
      switch (fan_mode)
      {
      case climate::CLIMATE_FAN_AUTO:
        frame_[10] = 0x00;
        break;
      case climate::CLIMATE_FAN_LOW: // silent / fan 1
        frame_[10] = 0x02;
        break;
      case climate::CLIMATE_FAN_MEDIUM: // Fan 3
        frame_[10] = 0x03;
        break;
      case climate::CLIMATE_FAN_HIGH: // Fan 5 / Turbo
        frame_[10] = 0x05;
        break;
      default:
        frame_[10] = 0x00; // fallback to Auto
        break;
      }

      ESP_LOGD(TAG, "Fan mode set in ESPHome: %s -> Frame byte[10] = 0x%02X",
               climate::climate_fan_mode_to_string(fan_mode), frame_[10]);
    }

    void RotensoFrameBuilder::update_checksum()
    {
      uint8_t checksum = 0x00;
      for (size_t i = 0; i < FRAME_LENGTH - 1; ++i)
      {
        checksum ^= frame_[i];
      }
      frame_[FRAME_LENGTH - 1] = checksum;
    }

    std::array<uint8_t, RotensoFrameBuilder::FRAME_LENGTH> RotensoFrameBuilder::build_frame()
    {
      update_checksum();
      return frame_;
    }

  } // namespace rotenso
} // namespace esphome
