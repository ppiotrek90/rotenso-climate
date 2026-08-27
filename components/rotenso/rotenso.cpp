#include "rotenso.h"
#include "frame_parser.h"
#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.climate";

static std::string vane_position_to_string(uint8_t raw) {
  switch (raw) {
    case 0x01: return "Top";
    case 0x02: return "Upper";
    case 0x03: return "Mid";
    case 0x04: return "Lower";
    case 0x05: return "Bottom";

    // The AC reports different values while the vane is in a MOVE mode.
    // These are the observed status counterparts of the commands 0x0D/0x15/0x1D.
    case 0x0C: return "Move Full";
    case 0x14: return "Move Upper";
    case 0x1C: return "Move Lower";

    // 0x08 has been observed, but its exact meaning is not confirmed yet.
    default: return "Unknown";
  }
}

void RotensoVaneSelect::control(size_t index) {
  if (this->parent_ == nullptr || !this->has_index(index)) {
    ESP_LOGW(TAG, "Invalid vertical vane select request: index=%zu", index);
    return;
  }

  const char *option = this->option_at(index);
  if (option == nullptr) {
    return;
  }

  this->parent_->control_vertical_vane(option);
}

void RotensoClimate::setup() {
  ESP_LOGI(TAG, "Rotenso climate setup complete");

  this->set_interval("heartbeat", 3000, [this]() {
    this->send_heartbeat();
  });
}

void RotensoClimate::loop() {
  // Collect UART bytes continuously. Do not wait an arbitrary 500 ms:
  // a heartbeat is 61 bytes and can be processed as soon as the full frame arrives.
  while (this->available()) {
    uint8_t byte;
    if (this->read_byte(&byte)) {
      this->rx_buffer_.push_back(byte);
    } else {
      break;
    }
  }

  while (!this->rx_buffer_.empty()) {
    // Synchronize to frame start.
    if (this->rx_buffer_.front() != 0xBB) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    // Header is BB, address, ..., command at byte 3.
    if (this->rx_buffer_.size() < 4) {
      return;
    }

    size_t expected_length = 0;
    if (this->rx_buffer_[3] == 0x04) {
      expected_length = 61;
    } else if (this->rx_buffer_[3] == 0x09) {
      expected_length = 51;
    } else {
      // Unknown frame type. Drop only the sync byte and try to resynchronize.
      ESP_LOGD(TAG, "Unknown UART frame command 0x%02X", this->rx_buffer_[3]);
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    if (this->rx_buffer_.size() < expected_length) {
      return;
    }

    std::vector<uint8_t> frame(
        this->rx_buffer_.begin(),
        this->rx_buffer_.begin() + expected_length);

    this->rx_buffer_.erase(
        this->rx_buffer_.begin(),
        this->rx_buffer_.begin() + expected_length);

    this->process_frame(frame);
  }
}

climate::ClimateTraits RotensoClimate::traits() {
  auto traits = climate::ClimateTraits();

  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_FAN_ONLY,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_AUTO,
  });

  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_QUIET,
  });

  traits.set_supported_presets({
      climate::CLIMATE_PRESET_NONE,
      climate::CLIMATE_PRESET_ECO,
      climate::CLIMATE_PRESET_BOOST,
      climate::CLIMATE_PRESET_SLEEP,
  });

  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(31);
  traits.set_visual_temperature_step(0.5);
  traits.set_visual_current_temperature_step(0.1);

  return traits;
}

void RotensoClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
  if (call.get_preset().has_value()) this->preset = *call.get_preset();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);

  auto frame = builder.build_frame();
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::control_vertical_vane(const std::string &position) {
  ESP_LOGI(TAG, "Vertical vane set to: %s", position.c_str());

  if (position == "Unknown") {
    ESP_LOGW(TAG, "Unknown is diagnostic-only and will not be transmitted");
    return;
  }

  this->vertical_vane_position_ = position;

  climate::ClimateCall call = this->make_call();
  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);

  auto frame = builder.build_frame();
  this->write_array(frame.data(), frame.size());

  if (this->vertical_vane_select_ != nullptr) {
    this->vertical_vane_select_->publish_state(position);
  }
}

void RotensoClimate::send_test_frame(uint8_t byte_index, uint8_t value) {
  climate::ClimateCall call = this->make_call();
  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);
  builder.set_raw_byte(byte_index, value);
  auto frame = builder.build_frame();
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame_or(uint8_t byte_index, uint8_t bits) {
  climate::ClimateCall call = this->make_call();
  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);
  builder.or_raw_byte(byte_index, bits);
  auto frame = builder.build_frame();
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame_or2(uint8_t byte_index1, uint8_t bits1, uint8_t byte_index2, uint8_t bits2) {
  climate::ClimateCall call = this->make_call();
  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);
  builder.or_raw_byte(byte_index1, bits1);
  builder.or_raw_byte(byte_index2, bits2);
  auto frame = builder.build_frame();
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_heartbeat() {
  ESP_LOGD(TAG, "Sending UART heartbeat");
  static const uint8_t heartbeat_packet[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
  this->write_array(heartbeat_packet, sizeof(heartbeat_packet));
  this->flush();
}

void RotensoClimate::process_frame(const std::vector<uint8_t> &frame) {
  std::string log_line;
  char byte_str[6];
  for (uint8_t byte : frame) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", byte);
    log_line += byte_str;
  }
  ESP_LOGD(TAG, "UART response: %s", log_line.c_str());

  if (frame.size() != 61 || frame[3] != 0x04) {
    ESP_LOGD(TAG, "Ignoring non-heartbeat frame");
    return;
  }

  auto parsed = parse_heartbeat(frame);
  if (!parsed.valid) return;

  this->mode = parsed.mode;
  this->fan_mode = parsed.fan_mode;
  this->target_temperature = parsed.temperature;
  this->current_temperature = parsed.current_temperature;
  this->preset = parsed.preset;

  this->vertical_vane_position_ = vane_position_to_string(parsed.vertical_vane_position_raw);

  ESP_LOGI(TAG, "Updated climate state from heartbeat");
  ESP_LOGD(TAG, "Vertical vane raw byte[51] = 0x%02X -> %s",
           parsed.vertical_vane_position_raw, this->vertical_vane_position_.c_str());

  if (this->vertical_vane_select_ != nullptr) {
    this->vertical_vane_select_->publish_state(this->vertical_vane_position_);
  }

  if (this->coil_temperature_sensor_ != nullptr) this->coil_temperature_sensor_->publish_state(parsed.coil_temperature);
  if (this->room_temperature_sensor_ != nullptr) this->room_temperature_sensor_->publish_state(parsed.current_temperature);
  if (this->error_binary_sensor_ != nullptr) this->error_binary_sensor_->publish_state(parsed.error_code != 0);
  if (this->anti_mildew_binary_sensor_ != nullptr) this->anti_mildew_binary_sensor_->publish_state(parsed.anti_mildew);

  this->publish_state();
}

}  // namespace rotenso
}  // namespace esphome
