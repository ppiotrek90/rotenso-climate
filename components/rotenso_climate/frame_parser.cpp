#include "frame_parser.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.parser";

ParsedClimateState parse_heartbeat(const std::vector<uint8_t> &buffer) {
    ParsedClimateState result;
  
    if (buffer.size() != 61 || buffer[3] != 0x04) {
      ESP_LOGW(TAG, "Invalid heartbeat packet");
      return result;
    }
  
    uint8_t state_mode_byte = buffer[7];  // adjust index if needed

    uint8_t state_nibble = (state_mode_byte & 0xF0) >> 4;
    uint8_t mode_nibble = (state_mode_byte & 0x0F);

    //parse climate mode
    if (state_nibble == 0x2) {
        result.mode = climate::CLIMATE_MODE_OFF;
      } else {
        switch (mode_nibble) {
          case 0x1: result.mode = climate::CLIMATE_MODE_COOL; break;
          case 0x2: result.mode = climate::CLIMATE_MODE_FAN_ONLY;  break;
          case 0x3: result.mode = climate::CLIMATE_MODE_DRY; break;
          case 0x4: result.mode = climate::CLIMATE_MODE_HEAT; break;
          case 0x5: result.mode = climate::CLIMATE_MODE_AUTO; break;
          default: result.mode = climate::CLIMATE_MODE_OFF; break;
        }
      }

      //parse climate preset
      switch (state_nibble) {
        case 0x2: result.preset = esphome::climate::CLIMATE_PRESET_NONE; break; 
        case 0x3: result.preset = esphome::climate::CLIMATE_PRESET_NONE; break;
        case 0x7: result.preset = esphome::climate::CLIMATE_PRESET_ECO; break;
        case 0xB: result.preset = esphome::climate::CLIMATE_PRESET_BOOST; break;
        case 0xF: result.preset = esphome::climate::CLIMATE_PRESET_BOOST; break; // on remote is selected eco + boost but AC works like it was on BOOST
        default: result.preset = esphome::climate::CLIMATE_PRESET_NONE; break;
      }

    //parse fan speed
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

    //parse temperature
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

    result.valid = true;
    return result;
  }

}  // namespace rotenso
}  // namespace esphome
