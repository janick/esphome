/*
  lanbon_l8_hd.cpp - Lanbon L8-HD dimmer support for ESPHome

  Copyright © 2025 Janick Bergeron
  Copyright © 2021 Anatoly Savchenkov
  Copyright © 2020 Jeff Rescignano

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  and associated documentation files (the “Software”), to deal in the Software without
  restriction, including without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  -----

  If modifying this file, in addition to the license above, please ensure to include links back to the original code:
  https://jeffresc.dev/blog/2020-10-10
  https://github.com/JeffResc/Sonoff-D1-Dimmer
  https://github.com/arendst/Tasmota/blob/2d4a6a29ebc7153dbe2717e3615574ac1c84ba1d/tasmota/xdrv_37_sonoff_d1.ino#L119-L131

  -----
*/

/*********************************************************************************************\
 * Lanbon L8-DS Dimmer
 *
 *  GPIO27 - Relay powering dimmer (0=off, 1=on)
 *  GPIO12 - UART TX dimmer control sequences (see below)
 *
 *  0  1  2  3     4  5  6  7  8  9  A  B  C  D  E  F 10
 *  20 20 EF 01    4D A3 00 00 00 00                        - Initialization
 *  EF 02 NN ED^NN                                          - Set dimmer percentage
\*********************************************************************************************/

#include "lanbon_l8_hd.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace lanbon_l8_hd {

static const char *const TAG = "lanbon_l8_hd";

// Set the device's traits
light::LightTraits LocalDimmerOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void LocalDimmerOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Lanbon L8-HD Dimmer: '%s'\n"
                "  Minimal brightness: %d"
                "  Maximal brightness: %d",
                this->light_state_ ? this->light_state_->get_name().c_str() : "", this->min_value_, this->max_value_);

  this->started_ = false;
}

void LocalDimmerOutput::write_state(light::LightState *state) {
  bool binary;
  float brightness;

  // Fill our variables with the device's current state
  state->current_values_as_binary(&binary);
  state->current_values_as_brightness(&brightness);

  // Convert ESPHome's brightness (0-1) to the device's internal brightness (0-100)
  uint8_t calculated_brightness = (uint8_t) roundf(brightness * 100);

  if (calculated_brightness == 0) {
    binary = false;
  } else {
    calculated_brightness = remap<uint8_t, uint8_t>(calculated_brightness, 0, 100, this->min_value_, this->max_value_);
  }

  // If a new value, write to the dimmer
  if (binary != this->last_binary_ || calculated_brightness != this->last_brightness_) {
    ESP_LOGV(TAG, "Setting dimmer state to %s, brightness=%d%%", ONOFF(binary), calculated_brightness);

    this->control_dimmer_(binary, calculated_brightness);
    this->last_binary_ = binary;
    this->last_brightness_ = calculated_brightness;
  }
}

void LocalDimmerOutput::start_dimmer_() {
  if (this->power_relay_ != nullptr)
    this->power_relay_->turn_on();

  uint8_t attn_command[1] = {0x20};
  this->write_command_(attn_command, sizeof(attn_command));
  this->write_command_(attn_command, sizeof(attn_command));

  uint8_t start_command[8] = {0xEF, 0x01, 0x4D, 0xA3, 0x00, 0x00, 0x00, 0x00};
  this->write_command_(start_command, sizeof(start_command));
}

void LocalDimmerOutput::control_dimmer_(const bool binary, const uint8_t brightness) {
  if (!binary) {
    // Send 0% brightness command to explicitly turn off dimmer MCU
    uint8_t cmd[4] = {0xEF, 0x02, 0x00, 0xED};
    ESP_LOGVV(TAG, "Setting dimmer state to %s, raw brightness=%d", ONOFF(binary), cmd[2]);
    this->write_command_(cmd, sizeof(cmd));

    if (this->power_relay_ != nullptr)
      this->power_relay_->turn_off();
    return;
  }

  if (this->last_binary_ == 0) {
    this->start_dimmer_();
  }

  uint8_t cmd[4] = {0xEF, 0x02, 0x00, 0xED};

  cmd[2] = brightness;
  cmd[3] ^= cmd[2];
  ESP_LOGVV(TAG, "Setting dimmer state to %s, raw brightness=%d", ONOFF(binary), cmd[2]);
  this->write_command_(cmd, sizeof(cmd));
}

void LocalDimmerOutput::write_command_(uint8_t *cmd, const size_t len) {
  ESP_LOGVV(TAG, "Writing to dimmer: %s", format_hex_pretty(cmd, len).c_str());
  this->write_array(cmd, len);
}

}  // namespace lanbon_l8_hd
}  // namespace esphome
