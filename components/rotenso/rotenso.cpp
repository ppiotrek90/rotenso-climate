#include "rotenso.h"
#include "frame_parser.h"
#include "rotenso_frame_builder.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rotenso {

static const char *const TAG = "rotenso.climate";

void RotensoClimate::setup() {
  ESP_LOGI(TAG, "Rotenso climate setup complete");

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

  // Simple on/off toggle on the climate card itself, alongside the detailed
  // "Vertical Vane"/"Horizontal Vane" selects. Vertical is confirmed working;
  // horizontal READ is confirmed, WRITE (byte 33) is an unconfirmed guess.
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
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }

  if (call.get_preset().has_value()) {
    this->preset = *call.get_preset();
  }

  if (call.get_swing_mode().has_value()) {
    climate::ClimateSwingMode swing = *call.get_swing_mode();
    this->swing_mode = swing;
    // Simple toggle, overrides whatever the detailed selects had, matching
    // the intent of a simple click on the climate card.
    bool want_vertical = (swing == climate::CLIMATE_SWING_VERTICAL || swing == climate::CLIMATE_SWING_BOTH);
    bool want_horizontal = (swing == climate::CLIMATE_SWING_HORIZONTAL || swing == climate::CLIMATE_SWING_BOTH);

    this->vertical_vane_position_ = want_vertical ? "Move Full" : "Off";
    this->horizontal_vane_position_ = want_horizontal ? "Move Full" : "Off";
    this->vane_command_sent_at_ = millis();

    if (this->vertical_vane_select_ != nullptr) {
      this->vertical_vane_select_->publish_state(static_cast<size_t>(want_vertical ? 6 : 0));
    }
    if (this->horizontal_vane_select_ != nullptr) {
      this->horizontal_vane_select_->publish_state(static_cast<size_t>(want_horizontal ? 6 : 0));
    }
  }

  RotensoFrameBuilder builder;

  builder.from_climate_state(this, call);

  // Preserve the currently selected vane position when another
  // Climate parameter is changed.
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);

  auto frame = builder.build_frame();

  this->write_array(frame.data(), frame.size());
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

void RotensoClimate::control_vertical_vane(size_t index) {
  // Select option indices:
  // 0 Off, 1 Top, 2 Upper, 3 Mid, 4 Lower, 5 Bottom,
  // 6 Move Full, 7 Move Upper, 8 Move Lower, 9 Unknown.
  if (index > 9) {
    ESP_LOGW(TAG, "Invalid vertical vane select index: %u", static_cast<unsigned>(index));
    return;
  }

  // Unknown is a diagnostic status, not a command.
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
  this->update_swing_mode_();
  this->vane_command_sent_at_ = millis();

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

  // Unknown is a diagnostic status, not a command.
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
  this->update_swing_mode_();
  this->vane_command_sent_at_ = millis();

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
  // Ignore a status frame that arrives too soon after we sent a vane
  // command - it may still be answering the previous heartbeat request
  // and would show a stale value, briefly flickering the select/swing
  // back to the old state before the next real heartbeat corrects it.
  if (millis() - this->vane_command_sent_at_ < 2000) {
    return;
  }

  // byte[51] packs TWO independent fields:
  //   bits 3-4 = move sub-mode (1=Full, 2=Upper, 3=Lower, 0=not moving)
  //   bits 0-2 = position anchor (1-5 = Top..Bottom, 0 = none)
  // When moving, the AC reports whatever position the vane started the
  // sweep from in bits 0-2 - that varies (we've seen 0x09, 0x0C/0x0D,
  // 0x14/0x15, 0x1C/0x1D), so the move sub-mode (bits 3-4) is what
  // actually identifies the mode; the position bits must be ignored for
  // that classification, not matched as part of one fixed byte value.
  uint8_t mv = (raw >> 3) & 0x03;
  uint8_t pos = raw & 0x07;

  if (mv == 0x01) {
    this->vertical_vane_position_ = "Move Full";
    if (this->vertical_vane_select_ != nullptr)
      this->vertical_vane_select_->publish_state(static_cast<size_t>(6));
    this->update_swing_mode_();
    return;
  }
  if (mv == 0x02) {
    this->vertical_vane_position_ = "Move Upper";
    if (this->vertical_vane_select_ != nullptr)
      this->vertical_vane_select_->publish_state(static_cast<size_t>(7));
    this->update_swing_mode_();
    return;
  }
  if (mv == 0x03) {
    this->vertical_vane_position_ = "Move Lower";
    if (this->vertical_vane_select_ != nullptr)
      this->vertical_vane_select_->publish_state(static_cast<size_t>(8));
    this->update_swing_mode_();
    return;
  }

  // mv == 0: fixed position (or fully off)
  switch (pos) {
    case 0x00:
      this->vertical_vane_position_ = "Off";
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(0));
      break;
    case 0x01:
      this->vertical_vane_position_ = "Top";
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(1));
      break;
    case 0x02:
      this->vertical_vane_position_ = "Upper";
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(2));
      break;
    case 0x03:
      this->vertical_vane_position_ = "Mid";
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(3));
      break;
    case 0x04:
      this->vertical_vane_position_ = "Lower";
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(4));
      break;
    case 0x05:
      this->vertical_vane_position_ = "Bottom";
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(5));
      break;
    default:
      ESP_LOGW(TAG, "Unknown vertical vane status: byte[51]=0x%02X", raw);
      if (this->vertical_vane_select_ != nullptr)
        this->vertical_vane_select_->publish_state(static_cast<size_t>(9));
      break;
  }
  this->update_swing_mode_();
}

void RotensoClimate::publish_horizontal_vane_state_(uint8_t raw) {
  // Ignore a status frame that arrives too soon after we sent a vane
  // command - same reasoning as publish_vertical_vane_state_().
  if (millis() - this->vane_command_sent_at_ < 2000) {
    return;
  }

  // Confirmed READ mapping: byte[52] packs the same two fields as byte[51]:
  //   bits 3-5 = move sub-mode (1=Full, 2=Left, 3=Mid, 4=Right, 0=not moving)
  //   bits 0-2 = position anchor (1-5 = Left..Right, 0 = none)
  uint8_t mv = (raw >> 3) & 0x07;
  uint8_t pos = raw & 0x07;

  if (mv == 0x01) {
    this->horizontal_vane_position_ = "Move Full";
    if (this->horizontal_vane_select_ != nullptr)
      this->horizontal_vane_select_->publish_state(static_cast<size_t>(6));
  } else if (mv == 0x02) {
    this->horizontal_vane_position_ = "Move Left";
    if (this->horizontal_vane_select_ != nullptr)
      this->horizontal_vane_select_->publish_state(static_cast<size_t>(7));
  } else if (mv == 0x03) {
    this->horizontal_vane_position_ = "Move Mid";
    if (this->horizontal_vane_select_ != nullptr)
      this->horizontal_vane_select_->publish_state(static_cast<size_t>(8));
  } else if (mv == 0x04) {
    this->horizontal_vane_position_ = "Move Right";
    if (this->horizontal_vane_select_ != nullptr)
      this->horizontal_vane_select_->publish_state(static_cast<size_t>(9));
  } else {
    // mv == 0: fixed position (or fully off)
    switch (pos) {
      case 0x00:
        this->horizontal_vane_position_ = "Off";
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(0));
        break;
      case 0x01:
        this->horizontal_vane_position_ = "Left";
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(1));
        break;
      case 0x02:
        this->horizontal_vane_position_ = "Mid-left";
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(2));
        break;
      case 0x03:
        this->horizontal_vane_position_ = "Mid";
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(3));
        break;
      case 0x04:
        this->horizontal_vane_position_ = "Mid-right";
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(4));
        break;
      case 0x05:
        this->horizontal_vane_position_ = "Right";
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(5));
        break;
      default:
        ESP_LOGW(TAG, "Unknown horizontal vane status: byte[52]=0x%02X", raw);
        if (this->horizontal_vane_select_ != nullptr)
          this->horizontal_vane_select_->publish_state(static_cast<size_t>(10));
        break;
    }
  }
  this->update_swing_mode_();
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

void RotensoClimate::send_test_frame(uint8_t byte_index, uint8_t value) {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);

  builder.set_raw_byte(byte_index, value);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];

  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGW(
      TAG,
      "TEST FRAME (byte[%d]=0x%02X): %s",
      byte_index,
      value,
      log_line.c_str());

  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame2(uint8_t byte_index1, uint8_t value1,
                                       uint8_t byte_index2, uint8_t value2) {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);

  builder.set_raw_byte(byte_index1, value1);
  builder.set_raw_byte(byte_index2, value2);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];

  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGW(
      TAG,
      "TEST FRAME2 (byte[%d]=0x%02X, byte[%d]=0x%02X): %s",
      byte_index1,
      value1,
      byte_index2,
      value2,
      log_line.c_str());

  this->write_array(frame.data(), frame.size());
}

void RotensoClimate::send_test_frame_or(uint8_t byte_index, uint8_t bits) {
  climate::ClimateCall call = this->make_call();

  RotensoFrameBuilder builder;
  builder.from_climate_state(this, call);
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);

  builder.or_raw_byte(byte_index, bits);

  auto frame = builder.build_frame();

  std::string log_line;
  char byte_str[6];

  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(byte_str, sizeof(byte_str), "0x%02X ", frame[i]);
    log_line += byte_str;
  }

  ESP_LOGW(
      TAG,
      "TEST FRAME OR (byte[%d]|=0x%02X): %s",
      byte_index,
      bits,
      log_line.c_str());

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
  builder.set_vertical_vane_position(this->vertical_vane_position_);
  builder.set_horizontal_vane_position(this->horizontal_vane_position_);

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
      byte_index1,
      bits1,
      byte_index2,
      bits2,
      log_line.c_str());

  this->write_array(frame.data(), frame.size());
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
      this->mode = parsed.mode;
      this->fan_mode = parsed.fan_mode;
      this->target_temperature = parsed.temperature;
      this->current_temperature = parsed.current_temperature;
      this->preset = parsed.preset;

      // byte[51] is the reported vertical vane position.
      this->publish_vertical_vane_state_(parsed.vertical_vane_position_raw);
      this->publish_horizontal_vane_state_(parsed.horizontal_vane_position_raw);

      ESP_LOGI(TAG, "Updated climate state from heartbeat");

      ESP_LOGD(
          TAG,
          "Vertical vane raw byte[51] = 0x%02X",
          parsed.vertical_vane_position_raw);

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

      this->publish_state();
    }
  }
}

}  // namespace rotenso
}  // namespace esphome