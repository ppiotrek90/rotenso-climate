#include "frame_parser.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.parser";

// XOR checksum of all bytes except the last one, which is the checksum itself.
// Same algorithm as the TCL-based protocol this device shares.
static uint8_t xor_checksum(const std::vector<uint8_t> &buffer) {
  uint8_t cs = 0;
  for (size_t i = 0; i + 1 < buffer.size(); i++) {
    cs ^= buffer[i];
  }
  return cs;
}

ParsedClimateState parse_heartbeat(const std::vector<uint8_t> &buffer) {
    ParsedClimateState result;
  
    if (buffer.size() != 61 || buffer[3] != 0x04) {
      ESP_LOGW(TAG, "Invalid heartbeat packet");
      return result;
    }

    uint8_t expected_cs = xor_checksum(buffer);
    if (expected_cs != buffer.back()) {
      ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X - discarding frame",
               expected_cs, buffer.back());
      return result;
    }
  
    uint8_t state_mode_byte = buffer[7];  // adjust index if needed

    uint8_t state_nibble = (state_mode_byte & 0xF0) >> 4;
    uint8_t mode_nibble = (state_mode_byte & 0x0F);

    // Parse climate mode.
    //
    // IMPORTANT: power state is encoded independently from the low mode
    // nibble. On this Rotenso/TCL variant an OFF status has been confirmed
    // as byte[7] = 0x05: the low nibble (0x5) looks like AUTO, but the high
    // state nibble is 0x0 and means the unit is actually OFF. Decoding the
    // low nibble first therefore makes ESPHome jump from OFF back to AUTO.
    //
    // state_nibble == 0x2 was the OFF value used by the original component;
    // keep it for compatibility with firmware/model variants.
    const bool power_off = (state_nibble == 0x0 || state_nibble == 0x2);

    if (power_off) {
      result.mode = climate::CLIMATE_MODE_OFF;
    } else {
      switch (mode_nibble) {
        case 0x1: result.mode = climate::CLIMATE_MODE_COOL; break;
        case 0x2: result.mode = climate::CLIMATE_MODE_FAN_ONLY; break;
        case 0x3: result.mode = climate::CLIMATE_MODE_DRY; break;
        case 0x4: result.mode = climate::CLIMATE_MODE_HEAT; break;
        case 0x5: result.mode = climate::CLIMATE_MODE_AUTO; break;
        default:
          ESP_LOGW(TAG,
                   "Unknown active climate mode: byte[7]=0x%02X (state=0x%X, mode=0x%X)",
                   state_mode_byte, state_nibble, mode_nibble);
          result.mode = climate::CLIMATE_MODE_OFF;
          break;
      }
    }

      // Parse climate preset
      switch (state_nibble) {
        case 0x2: result.preset = esphome::climate::CLIMATE_PRESET_NONE; break; 
        case 0x3: result.preset = esphome::climate::CLIMATE_PRESET_NONE; break;
        case 0x7: result.preset = esphome::climate::CLIMATE_PRESET_ECO; break;
        case 0xB: result.preset = esphome::climate::CLIMATE_PRESET_BOOST; break;
        case 0xF: result.preset = esphome::climate::CLIMATE_PRESET_BOOST; break; // on remote is selected eco + boost but AC works like it was on BOOST
        default: result.preset = esphome::climate::CLIMATE_PRESET_NONE; break;
      }

    // Parse fan speed
    uint8_t fan_raw = (buffer[8] >> 4) & 0x0F;
    switch (fan_raw) {
      case 0x8: result.fan_mode = climate::CLIMATE_FAN_AUTO; break; // Fan Auto
      case 0x9: result.fan_mode = climate::CLIMATE_FAN_LOW; break; // Fan 1 / Silent
      case 0xC: result.fan_mode = climate::CLIMATE_FAN_LOW; break; // Fan 2
      case 0xA: result.fan_mode = climate::CLIMATE_FAN_MEDIUM; break; // Fan 3
      case 0xD: result.fan_mode = climate::CLIMATE_FAN_HIGH; break;  // Map Fan 4 to HIGH
      case 0xB: result.fan_mode = climate::CLIMATE_FAN_HIGH; break;  // Max fan
      default: result.fan_mode = climate::CLIMATE_FAN_AUTO; break;
    }

    // Quiet fan overrides whatever speed nibble says - byte 33, bit 0x80.
    // Confirmed against a real remote toggle.
    bool quiet_fan = (buffer[33] & 0x80) != 0;
    if (quiet_fan) {
      result.fan_mode = climate::CLIMATE_FAN_QUIET;
    }

    // Parse temperature
    uint8_t fan_temp_byte = buffer[8];
    uint8_t temp_decimal_byte = buffer[9];

    // Extract temperature whole part
    uint8_t temp_whole = fan_temp_byte & 0x0F;  // lower 4 bits
    float temperature = 16 + static_cast<float>(temp_whole);

    // Add decimal part
    if ((temp_decimal_byte & 0x0F) == 0x2) {
      temperature += 0.5f;
    }

    result.temperature = temperature;

    // Parse current (room) temperature - bytes 17-18, 16-bit big-endian,
    // same encoding as the TCL-based protocol this device shares (Fahrenheit-derived).
    uint16_t curr_temp_raw = (static_cast<uint16_t>(buffer[17]) << 8) | buffer[18];
    result.current_temperature = (curr_temp_raw / 374.0f - 32.0f) / 1.8f;

    // Diagnostic-only: coil temp from a single byte
    result.coil_temperature = ((static_cast<uint16_t>(buffer[30]) << 8) / 374.0f - 32.0f) / 1.8f;

    // Diagnostic-only: error code, raw byte, meaning of non-zero values unverified.
    result.error_code = buffer[16];

    // Anti-mildew
    result.anti_mildew = (buffer[9] & 0x08) != 0;

    // Display status, confirmed from RX captures.
    result.display = (buffer[7] & 0x20) != 0;

    // Vane position
    result.vertical_vane_position_raw = buffer[51];
    result.horizontal_vane_position_raw = buffer[52];

    // Sleep preset
    bool sleep_on = (buffer[19] & 0x01) != 0;
    if (result.preset == climate::CLIMATE_PRESET_NONE && sleep_on) {
      result.preset = climate::CLIMATE_PRESET_SLEEP;
    }

    result.valid = true;
    return result;
  }

}  // namespace rotenso
}  // namespace esphome