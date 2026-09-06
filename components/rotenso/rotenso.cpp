#include "rotenso.h"
#include "frame_parser.h"
#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.climate";

void RotensoClimate::setup() {
  ESP_LOGI(TAG, "Rotenso climate setup complete");

  // Buzzer state is not available in the RX status frame. Keep a safe,
  // explicit default and expose it immediately after startup.
  if (this->buzzer_switch_ != nullptr) {
    this->buzzer_switch_->publish_state(this->buzzer_state_);
  }

  this->send_heartbeat();

  this->set_interval("heartbeat", 3000, [this]() {
    this->send_heartbeat();
  });
}

void RotensoClimate::loop() {
  static int32_t response_start_time = -1;

  if (response_start_time == -1 && this->available()) {
    response_start_time = millis();
    ESP_LOGD(TAG, "UART data detected, waiting to collect full response...");
  }

  if (response_start_time != -1 &&
      millis() - response_start_time >= 500) {
    this->parse_uart_response();
    response_start_time = -1;
  }
}


climate::ClimateTraits RotensoClimate::traits() {
  auto traits = climate::ClimateTraits();

  traits.add_feature_flags(
      climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

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

  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_HORIZONTAL,
      climate::CLIMATE_SWING_BOTH,
  });

  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(31);
  traits.set_visual_temperature_step(0.5);

  return traits;
}

void RotensoClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    this->pending_mode_ = this->mode;
    this->pending_mode_valid_ = true;
    this->pending_mode_sent_at_ = millis();
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
    this->pending_target_temperature_ = this->target_temperature;
    this->pending_target_temperature_valid_ = true;
    this->pending_target_temperature_sent_at_ = millis();
  }

  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
    this->pending_fan_mode_ = *this->fan_mode;
    this->pending_fan_mode_valid_ = true;
    this->pending_fan_mode_sent_at_ = millis();
  }

  if (call.get_preset().has_value()) {
    this->preset = *call.get_preset();
  }

  if (call.get_swing_mode().has_value()) {
    climate::ClimateSwingMode swing = *call.get_swing_mode();
    this->swing_mode = swing;

    bool want_vertical = (swing == climate::CLIMATE_SWING_VERTICAL || swing == climate::CLIMATE_SWING_BOTH);
    bool want_horizontal = (swing == climate::CLIMATE_SWING_HORIZONTAL || swing == climate::CLIMATE_SWING_BOTH);

    this->vertical_vane_position_ = want_vertical ? "Move Full" : "Off";
    this->horizontal_vane_position_ = want_horizontal ? "Move Full" : "Off";
    this->pending_vertical_vane_position_ = this->vertical_vane_position_;
    this->pending_vertical_vane_valid_ = true;
    this->pending_vertical_vane_sent_at_ = millis();
    this->pending_horizontal_vane_position_ = this->horizontal_vane_position_;
    this->pending_horizontal_vane_valid_ = true;
    this->pending_horizontal_vane_sent_at_ = millis();

    this->last_published_vertical_vane_index_ = static_cast<size_t>(want_vertical ? 6 : 0);
    if (this->vertical_vane_select_ != nullptr) {
      this->vertical_vane_select_->publish_state(this->last_published_vertical_vane_index_);
    }
    this->last_published_horizontal_vane_index_ = static_cast<size_t>(want_horizontal ? 6 : 0);
    if (this->horizontal_vane_select_ != nullptr) {
      this->horizontal_vane_select_->publish_state(this->last_published_horizontal_vane_index_);
    }
  }

  RotensoFrameBuilder builder;

  builder.from_climate_state(this, call);
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);
  if (this->anti_mildew_state_valid_) {
    builder.set_anti_mildew(this->anti_mildew_state_);
  }
  if (this->buzzer_state_valid_) {
    builder.set_buzzer(this->buzzer_state_);
  }
  if (this->display_state_valid_) {
    builder.set_display(this->display_state_);
  }

  auto frame = builder.build_frame();

  this->write_array(frame.data(), frame.size());
  this->publish_state();
}

void RotensoVerticalVaneSelect::control(size_t index) {
  if (this->parent_ == nullptr) {
    ESP_LOGW("rotenso.climate", "Vertical vane select has no parent");
    return;
  }

  this->parent_->control_vertical_vane(index);
}

void RotensoHorizontalVaneSelect::control(size_t index) {
  if (this->parent_ == nullptr) {
    ESP_LOGW("rotenso.climate", "Horizontal vane select has no parent");
    return;
  }

  this->parent_->control_horizontal_vane(index);
}

void RotensoAntiMildewSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGW("rotenso.climate", "Anti-mildew switch has no parent");
    return;
  }

  this->parent_->control_anti_mildew(state);
}

void RotensoClimate::control_anti_mildew(bool state) {
  ESP_LOGI(TAG, "Anti-mildew set to: %s", state ? "On" : "Off");

  this->anti_mildew_state_ = state;
  this->anti_mildew_state_valid_ = true;

  if (this->anti_mildew_switch_ != nullptr) {
    this->anti_mildew_switch_->publish_state(state);
  }

  this->send_current_state_frame_();
}

void RotensoBuzzerSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGW("rotenso.climate", "Buzzer switch has no parent");
    return;
  }

  this->parent_->control_buzzer(state);
}

void RotensoClimate::control_buzzer(bool state) {
  ESP_LOGI(TAG, "Buzzer set to: %s", state ? "On" : "Off");

  this->buzzer_state_ = state;
  this->buzzer_state_valid_ = true;

  if (this->buzzer_switch_ != nullptr) {
    this->buzzer_switch_->publish_state(state);
  }

  this->send_current_state_frame_();
}

void RotensoDisplaySwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGW("rotenso.climate", "Display switch has no parent");
    return;
  }

  this->parent_->control_display(state);
}

void RotensoClimate::control_display(bool state) {
  ESP_LOGI(TAG, "Display set to: %s", state ? "On" : "Off");

  this->display_state_ = state;
  this->display_state_valid_ = true;

  if (this->display_switch_ != nullptr) {
    this->display_switch_->publish_state(state);
  }

  this->send_current_state_frame_();
}

void RotensoClimate::control_vertical_vane(size_t index) {
  // Select option indices:
  // 0 Off, 1 Top, 2 Upper, 3 Mid, 4 Lower, 5 Bottom,
  // 6 Move Full, 7 Move Upper, 8 Move Lower, 9 Unknown.
  if (index > 9) {
    ESP_LOGW(TAG, "Invalid vertical vane select index: %u", static_cast<unsigned>(index));
    return;
  }

  if (index == 9) {
    ESP_LOGW(TAG, "Ignoring attempt to select Unknown vertical vane state");
    return;
  }

  static const char *const positions[] = {
      "Off", "Top", "Upper", "Mid", "Lower", "Bottom",
      "Move Full", "Move Upper", "Move Lower",
  };

  const std::string position = positions[index];
  ESP_LOGI(TAG, "Vertical vane set to: %s", position.c_str());

  this->vertical_vane_position_ = position;
  this->pending_vertical_vane_position_ = position;
  this->pending_vertical_vane_valid_ = true;
  this->pending_vertical_vane_sent_at_ = millis();
  this->update_swing_mode_();
  this->last_published_vertical_vane_index_ = index;

  if (this->vertical_vane_select_ != nullptr) {
    this->vertical_vane_select_->publish_state(index);
  }

  this->publish_state();
  this->send_current_state_frame_();
}

void RotensoClimate::control_horizontal_vane(size_t index) {
  // Select option indices:
  // 0 Off, 1 Left, 2 Mid-left, 3 Mid, 4 Mid-right, 5 Right,
  // 6 Move Full, 7 Move Left, 8 Move Mid, 9 Move Right, 10 Unknown.
  if (index > 10) {
    ESP_LOGW(TAG, "Invalid horizontal vane select index: %u", static_cast<unsigned>(index));
    return;
  }

  if (index == 10) {
    ESP_LOGW(TAG, "Ignoring attempt to select Unknown horizontal vane state");
    return;
  }

  static const char *const positions[] = {
      "Off", "Left", "Mid-left", "Mid", "Mid-right", "Right",
      "Move Full", "Move Left", "Move Mid", "Move Right",
  };

  const std::string position = positions[index];
  ESP_LOGI(TAG, "Horizontal vane set to: %s", position.c_str());

  this->horizontal_vane_position_ = position;
  this->pending_horizontal_vane_position_ = position;
  this->pending_horizontal_vane_valid_ = true;
  this->pending_horizontal_vane_sent_at_ = millis();
  this->update_swing_mode_();
  this->last_published_horizontal_vane_index_ = index;

  if (this->horizontal_vane_select_ != nullptr) {
    this->horizontal_vane_select_->publish_state(index);
  }

  this->publish_state();
  this->send_current_state_frame_();
}

void RotensoClimate::send_current_state_frame_() {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);
  if (this->anti_mildew_state_valid_) {
    builder.set_anti_mildew(this->anti_mildew_state_);
  }
  if (this->buzzer_state_valid_) {
    builder.set_buzzer(this->buzzer_state_);
  }
  if (this->display_state_valid_) {
    builder.set_display(this->display_state_);
  }

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

void RotensoClimate::publish_vertical_vane_state_(uint8_t raw) {

  uint8_t mv = (raw >> 3) & 0x03;
  uint8_t pos = raw & 0x07;

  std::string position;
  size_t index;

  if (mv == 0x01) {
    position = "Move Full";
    index = 6;
  } else if (mv == 0x02) {
    position = "Move Upper";
    index = 7;
  } else if (mv == 0x03) {
    position = "Move Lower";
    index = 8;
  } else {
    switch (pos) {
      case 0x00:
        position = "Off";
        index = 0;
        break;
      case 0x01:
        position = "Top";
        index = 1;
        break;
      case 0x02:
        position = "Upper";
        index = 2;
        break;
      case 0x03:
        position = "Mid";
        index = 3;
        break;
      case 0x04:
        position = "Lower";
        index = 4;
        break;
      case 0x05:
        position = "Bottom";
        index = 5;
        break;
      default:
        ESP_LOGW(TAG, "Unknown vertical vane status: byte[51]=0x%02X", raw);
        position = "Unknown";
        index = 9;
        break;
    }
  }

  if (this->pending_vertical_vane_valid_) {
    if (position == this->pending_vertical_vane_position_) {
      this->pending_vertical_vane_valid_ = false;
    } else if (!this->pending_timed_out_(this->pending_vertical_vane_sent_at_)) {
      return;
    } else {
      this->pending_vertical_vane_valid_ = false;
    }
  }

  this->vertical_vane_position_ = position;
  this->update_swing_mode_();

  if (index != this->last_published_vertical_vane_index_) {
    this->last_published_vertical_vane_index_ = index;
    if (this->vertical_vane_select_ != nullptr)
      this->vertical_vane_select_->publish_state(index);
  }
}

void RotensoClimate::publish_horizontal_vane_state_(uint8_t raw) {

  uint8_t mv = (raw >> 3) & 0x07;
  uint8_t pos = raw & 0x07;

  std::string position;
  size_t index;

  if (mv == 0x01) {
    position = "Move Full";
    index = 6;
  } else if (mv == 0x02) {
    position = "Move Left";
    index = 7;
  } else if (mv == 0x03) {
    position = "Move Mid";
    index = 8;
  } else if (mv == 0x04) {
    position = "Move Right";
    index = 9;
  } else {
    switch (pos) {
      case 0x00:
        position = "Off";
        index = 0;
        break;
      case 0x01:
        position = "Left";
        index = 1;
        break;
      case 0x02:
        position = "Mid-left";
        index = 2;
        break;
      case 0x03:
        position = "Mid";
        index = 3;
        break;
      case 0x04:
        position = "Mid-right";
        index = 4;
        break;
      case 0x05:
        position = "Right";
        index = 5;
        break;
      default:
        ESP_LOGW(TAG, "Unknown horizontal vane status: byte[52]=0x%02X", raw);
        position = "Unknown";
        index = 10;
        break;
    }
  }

  if (this->pending_horizontal_vane_valid_) {
    if (position == this->pending_horizontal_vane_position_) {
      this->pending_horizontal_vane_valid_ = false;
    } else if (!this->pending_timed_out_(this->pending_horizontal_vane_sent_at_)) {
      return;
    } else {
      this->pending_horizontal_vane_valid_ = false;
    }
  }

  this->horizontal_vane_position_ = position;
  this->update_swing_mode_();

  if (index != this->last_published_horizontal_vane_index_) {
    this->last_published_horizontal_vane_index_ = index;
    if (this->horizontal_vane_select_ != nullptr)
      this->horizontal_vane_select_->publish_state(index);
  }
}

bool RotensoClimate::pending_timed_out_(uint32_t sent_at) const {
  return millis() - sent_at >= PENDING_TIMEOUT_MS;
}

void RotensoClimate::update_swing_mode_() {
  bool vertical_moving = this->vertical_vane_position_.rfind("Move", 0) == 0;
  bool horizontal_moving = this->horizontal_vane_position_.rfind("Move", 0) == 0;

  if (vertical_moving && horizontal_moving) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (vertical_moving) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (horizontal_moving) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
}

void RotensoClimate::send_heartbeat() {
  ESP_LOGD(TAG, "Sending UART heartbeat");

  static const uint8_t heartbeat_packet[] = {
      0xBB, 0x00, 0x01, 0x04,
      0x02, 0x01, 0x00, 0xBD};

  this->write_array(
      heartbeat_packet,
      sizeof(heartbeat_packet));

  this->flush();

  delay(30);
}

void RotensoClimate::parse_uart_response() {
  size_t len = this->available();

  if (len == 0) {
    return;
  }

  std::vector<uint8_t> buffer;
  buffer.reserve(len);

  for (size_t i = 0; i < len; i++) {
    uint8_t byte;

    if (this->read_byte(&byte)) {
      buffer.push_back(byte);
    }
  }

  std::string log_line;
  char byte_str[6];

  for (size_t i = 0; i < buffer.size(); i++) {
    snprintf(
        byte_str,
        sizeof(byte_str),
        "0x%02X ",
        buffer[i]);

    log_line += byte_str;
  }

  ESP_LOGD(TAG, "UART response: %s", log_line.c_str());

  // Heartbeat/status response:
  // 61 bytes, command 0x04.
  //
  // Other frame types are currently ignored.
  if (buffer.size() == 61 && buffer[3] == 0x04) {
    auto parsed = parse_heartbeat(buffer);

    if (parsed.valid) {
      climate::ClimateMode old_mode = this->mode;
      optional<climate::ClimateFanMode> old_fan_mode = this->fan_mode;
      float old_target_temperature = this->target_temperature;
      float old_current_temperature = this->current_temperature;
      optional<climate::ClimatePreset> old_preset = this->preset;
      climate::ClimateSwingMode old_swing_mode = this->swing_mode;

      if (this->pending_mode_valid_) {
        if (parsed.mode == this->pending_mode_) {
          this->pending_mode_valid_ = false;
          this->mode = parsed.mode;
        } else if (this->pending_timed_out_(this->pending_mode_sent_at_)) {
          this->pending_mode_valid_ = false;
          this->mode = parsed.mode;
        }
      } else {
        this->mode = parsed.mode;
      }

      if (this->pending_target_temperature_valid_) {
        if (fabsf(parsed.temperature - this->pending_target_temperature_) < 0.01f) {
          this->pending_target_temperature_valid_ = false;
          this->target_temperature = parsed.temperature;
        } else if (this->pending_timed_out_(this->pending_target_temperature_sent_at_)) {
          this->pending_target_temperature_valid_ = false;
          this->target_temperature = parsed.temperature;
        }
      } else {
        this->target_temperature = parsed.temperature;
      }

      if (this->pending_fan_mode_valid_) {
        if (parsed.fan_mode == this->pending_fan_mode_) {
          this->pending_fan_mode_valid_ = false;
          this->fan_mode = parsed.fan_mode;
        } else if (this->pending_timed_out_(this->pending_fan_mode_sent_at_)) {
          this->pending_fan_mode_valid_ = false;
          this->fan_mode = parsed.fan_mode;
        }
      } else {
        this->fan_mode = parsed.fan_mode;
      }
      this->current_temperature = parsed.current_temperature;
      // NOTE: deliberately NOT syncing this->preset from parsed.preset here.
      // That READ-side decode (state_nibble in the STATUS frame) was never
      // independently confirmed and appears unreliable specifically for
      // ECO - it kept reporting ECO even after we sent the confirmed OFF
      // bit, silently reverting the user's own choice on the very next
      // heartbeat (the exact same bug we already fixed for anti-mildew).
      // preset is now purely a sticky, software-owned value like the vane
      // positions/anti-mildew/buzzer/display - only changed via control().

      // byte[51] is the reported vertical vane position.
      this->publish_vertical_vane_state_(parsed.vertical_vane_position_raw);
      this->publish_horizontal_vane_state_(parsed.horizontal_vane_position_raw);

      ESP_LOGI(TAG, "Updated climate state from heartbeat");

      if (this->coil_temperature_sensor_ != nullptr) {
        this->coil_temperature_sensor_->publish_state(
            parsed.coil_temperature);
      }

      if (this->room_temperature_sensor_ != nullptr) {
        this->room_temperature_sensor_->publish_state(
            parsed.current_temperature);
      }

      if (this->error_binary_sensor_ != nullptr) {
        this->error_binary_sensor_->publish_state(
            parsed.error_code != 0);
      }

      if (this->anti_mildew_binary_sensor_ != nullptr) {
        this->anti_mildew_binary_sensor_->publish_state(
            parsed.anti_mildew);
      }

      // Anti-mildew has a confirmed RX status bit (byte[9] bit 0x08).
      // Use the first valid heartbeat to initialize the switch from the AC
      // instead of inventing a default. During normal operation the same bit
      // can represent the currently active self-drying process, so it is also
      // exposed separately through the binary sensor and must not overwrite
      // the user's persistent command on every heartbeat.
      // Display has a confirmed RX status bit and is always synchronized
      // from the AC so startup and external changes are reflected in HA.
      this->display_state_ = parsed.display;
      this->display_state_valid_ = true;
      if (this->display_switch_ != nullptr) {
        this->display_switch_->publish_state(parsed.display);
      }

      if (!this->anti_mildew_state_valid_) {
        this->anti_mildew_state_ = parsed.anti_mildew;
        this->anti_mildew_state_valid_ = true;
        if (this->anti_mildew_switch_ != nullptr) {
          this->anti_mildew_switch_->publish_state(parsed.anti_mildew);
        }
        ESP_LOGI(TAG, "Initialized anti-mildew from AC: %s",
                 parsed.anti_mildew ? "On" : "Off");
      }

      bool changed =
          old_mode != this->mode ||
          old_fan_mode != this->fan_mode ||
          old_target_temperature != this->target_temperature ||
          old_current_temperature != this->current_temperature ||
          old_preset != this->preset ||
          old_swing_mode != this->swing_mode;

      if (changed) {
        this->publish_state();
      }
    }
  }
}

}  // namespace rotenso
}  // namespace esphome