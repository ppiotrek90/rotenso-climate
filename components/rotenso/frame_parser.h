#pragma once

#include <vector>
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace rotenso {

struct ParsedClimateState {
    bool valid = false;
    esphome::climate::ClimateMode mode;
    esphome::climate::ClimateFanMode fan_mode;
    float temperature;
    float current_temperature;
    esphome::climate::ClimatePreset preset = esphome::climate::CLIMATE_PRESET_NONE;
    // Diagnostic-only.
    float coil_temperature;
    // Diagnostic-only.
    uint8_t error_code;
    // Anti-mildew.
    bool anti_mildew;
    // Display status.
    bool display;
    // Health status.
    bool health;
    // Vertical vane position.
    uint8_t vertical_vane_position_raw;
    // Horizontal vane position.
    uint8_t horizontal_vane_position_raw;
};
      

ParsedClimateState parse_heartbeat(const std::vector<uint8_t> &buffer);

}  // namespace rotenso
}  // namespace esphome