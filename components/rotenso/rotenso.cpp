#include "rotenso.h"

#include <algorithm>
#include "frame_parser.h"
#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.climate";
static constexpr uint32_t HEARTBEAT_INTERVAL_ID = 0x524F5445;  // "ROTE"

static const char *vane_position_to_string(uint8_t raw) {
  switch (raw) {
    case 0x01: return "Top";
    case 0x02: return "Upper";
    case 0x03: return "Mid";
    case 0x04: return "Lower";
    case 0x05: return "Bottom";
    case 0x0D: return "Move Full";
    case 0x15: return "Move Upper";
    case 0x1D: return "Move Lower";
    default: return "Off";
  }
}

void RotensoVaneSelect::control(size_t index) {
  if (this->parent_ == nullptr) {
    ESP_LOGW("rotenso.select", "Vane select has no parent");
    return;
  }

  const char *option = this->option_at(index);
  if (option == nullptr) {
    ESP_LOGW("rotenso.select", "Invalid vane select index: %u", static_cast<unsigned>(index));
    return;
  }

  this->parent_->control_vertical_vane(option);
}

void RotensoClimate::setup() {
  ESP_LOGI(TAG, "Rotenso climate setup complete");
  this->rx_buffer_.reserve(128);

  this->set_interval(HEARTBEAT_INTERVAL_ID, 3000, [this]() {
    this->send_heartbeat();
  });
}

void RotensoClimate::loop() {
  // Collect bytes continuously instead of waiting an arbitrary 500 ms.
  // This lets us handle partial frames and multiple consecutive frames.
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }

    // The protocol uses 0xBB as the frame start. Discard noise before it.
    if (this->rx_buffer_.empty()) {
      if (byte != FRAME_START) {
        continue;
      }
    }

    this->rx_buffer_.push_back(byte);

    // Keep the buffer bounded if unexpected data is received.
    if (this->rx_buffer_.size() > 128) {
      auto start = std::find(
          this->rx_buffer_.begin() + 1,
          this->rx_buffer_.end(),
          FRAME_START);

      if (start != this->rx_buffer_.end()) {
        this->rx_buffer_.erase(this->rx_buffer_.begin(), start);
      } else {
        this->rx_buffer_.clear();
      }
    }
  }

  this->parse_uart_response();
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
  traits.set_visual_temperature_step(0.5f);

  return traits;
}

void RotensoClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }

  if (call.get_preset().has_value()) {
    this->preset = *call.get_preset();
  }

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);

  // Preserve the currently selected vane position when another Climate
  // parameter is changed.
  builder.set_vane_position(this->vertical_vane_position_);

  auto frame = builder.build_frame();
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::control_vertical_vane(const std::string &position) {
  ESP_LOGI(TAG, "Vertical vane set to: %s", position.c_str());

  this->vertical_vane_position_ = position;

  if (this->vertical_vane_select_ != nullptr) {
    this->vertical_vane_select_->publish_state(position);
  }

  // Build a complete SET frame using the current climate state.
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);

  // IMPORTANT: preserve the experimentally confirmed two-byte vane logic:
  // byte[32] = vane position
  // byte[10] |= 0x38 for Move modes.
  builder.set_vane_position(this->vertical_vane_position_);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];
  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGD(TAG, "VANE FRAME: %s", log_line.c_str());
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame(uint8_t byte_index, uint8_t value) {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);
  builder.set_raw_byte(byte_index, value);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];
  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGW(TAG, "TEST FRAME (byte[%d]=0x%02X): %s", byte_index, value, log_line.c_str());
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame_or(uint8_t byte_index, uint8_t bits) {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);
  builder.or_raw_byte(byte_index, bits);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];
  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGW(TAG, "TEST FRAME OR (byte[%d]|=0x%02X): %s", byte_index, bits, log_line.c_str());
  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame_or2(
    uint8_t byte_index1,
    uint8_t bits1,
    uint8_t byte_index2,
    uint8_t bits2) {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vane_position(this->vertical_vane_position_);
  builder.or_raw_byte(byte_index1, bits1);
  builder.or_raw_byte(byte_index2, bits2);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];
  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGW(
      TAG,
      "TEST FRAME OR2 (byte[%d]|=0x%02X, byte[%d]|=0x%02X): %s",
      byte_index1, bits1, byte_index2, bits2, log_line.c_str());

  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_heartbeat() {
  ESP_LOGD(TAG, "Sending UART heartbeat");

  static const uint8_t heartbeat_packet[] = {
      0xBB, 0x00, 0x01, 0x04,
      0x02, 0x01, 0x00, 0xBD};

  this->write_array(heartbeat_packet, sizeof(heartbeat_packet));
  this->flush();
  delay(30);
}

void RotensoClimate::parse_uart_response() {
  while (true) {
    if (this->rx_buffer_.empty()) {
      return;
    }

    // Re-synchronise on frame start.
    if (this->rx_buffer_.front() != FRAME_START) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    // Need enough bytes to read the command field at byte[3].
    if (this->rx_buffer_.size() < 4) {
      return;
    }

    size_t frame_length = 0;
    switch (this->rx_buffer_[3]) {
      case HEARTBEAT_COMMAND:
        frame_length = HEARTBEAT_FRAME_LENGTH;
        break;

      case SECONDARY_COMMAND:
        // Known 0x09 response length. We currently don't decode it,
        // but consuming it prevents it from blocking the next heartbeat.
        frame_length = SECONDARY_FRAME_LENGTH;
        break;

      default: {
        // Unknown frame type. Search for the next possible frame start.
        auto next = std::find(
            this->rx_buffer_.begin() + 1,
            this->rx_buffer_.end(),
            FRAME_START);

        if (next != this->rx_buffer_.end()) {
          this->rx_buffer_.erase(this->rx_buffer_.begin(), next);
          continue;
        }

        this->rx_buffer_.clear();
        return;
      }
    }

    if (this->rx_buffer_.size() < frame_length) {
      return;
    }

    std::vector<uint8_t> frame(
        this->rx_buffer_.begin(),
        this->rx_buffer_.begin() + frame_length);

    this->rx_buffer_.erase(
        this->rx_buffer_.begin(),
        this->rx_buffer_.begin() + frame_length);

    std::string log_line;
    char byte_str[6];
    for (uint8_t byte : frame) {
      snprintf(byte_str, sizeof(byte_str), "0x%02X ", byte);
      log_line += byte_str;
    }
    ESP_LOGD(TAG, "UART response: %s", log_line.c_str());

    if (frame.size() == HEARTBEAT_FRAME_LENGTH &&
        frame[3] == HEARTBEAT_COMMAND) {
      auto parsed = parse_heartbeat(frame);
      if (!parsed.valid) {
        continue;
      }

      this->mode = parsed.mode;
      this->fan_mode = parsed.fan_mode;
      this->target_temperature = parsed.temperature;
      this->current_temperature = parsed.current_temperature;
      this->preset = parsed.preset;

      this->vertical_vane_position_ = vane_position_to_string(
          parsed.vertical_vane_position_raw);

      ESP_LOGI(TAG, "Updated climate state from heartbeat");
      ESP_LOGD(
          TAG,
          "Vertical vane raw byte[51] = 0x%02X -> %s",
          parsed.vertical_vane_position_raw,
          this->vertical_vane_position_.c_str());

      if (this->vertical_vane_select_ != nullptr) {
        this->vertical_vane_select_->publish_state(
            this->vertical_vane_position_);
      }

      if (this->coil_temperature_sensor_ != nullptr) {
        this->coil_temperature_sensor_->publish_state(parsed.coil_temperature);
      }

      if (this->room_temperature_sensor_ != nullptr) {
        this->room_temperature_sensor_->publish_state(parsed.current_temperature);
      }

      if (this->error_binary_sensor_ != nullptr) {
        this->error_binary_sensor_->publish_state(parsed.error_code != 0);
      }

      if (this->anti_mildew_binary_sensor_ != nullptr) {
        this->anti_mildew_binary_sensor_->publish_state(parsed.anti_mildew);
      }

      this->publish_state();
    }
  }
}

}  // namespace rotenso
}  // namespace esphome
