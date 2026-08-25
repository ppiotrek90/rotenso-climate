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
    esphome::climate::ClimatePreset preset = esphome::climate::CLIMATE_PRESET_NONE;
};
      

ParsedClimateState parse_heartbeat(const std::vector<uint8_t> &buffer);

}  // namespace rotenso
}  // namespace esphome