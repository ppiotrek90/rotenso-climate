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
    // Diagnostic-only, not shown as current_temperature: byte 30, likely an internal
    // coil sensor, Not verified against a real thermometer - treat as approximate.
    float coil_temperature;
    // Diagnostic-only: byte 16, likely an error code (0 = no error)
    // Non-zero values are unverified on this device.
    uint8_t error_code;
};
      

ParsedClimateState parse_heartbeat(const std::vector<uint8_t> &buffer);

}  // namespace rotenso
}  // namespace esphome