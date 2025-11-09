#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace arv_rs485_logger {

static const char *const TAG = "arv_rs485_logger";

// AA <len> ... <CRC_L> <CRC_H>
class ArvRs485Logger : public Component, public uart::UARTDevice {
 public:
  // ESPHome constructs with uart.register_uart_device(var, config)
  ArvRs485Logger() = default;

  void setup() override {
    // nothing for now
  }

  void loop() override {
    // Read whatever is available
    while (this->available()) {
      uint8_t b;
      if (!this->read_byte(&b)) break;
      buffer_.push_back(b);

      // Raw byte trace (comment out later)
      ESP_LOGV(TAG, "RX 0x%02X", b);

      // Resync to 0xAA start
      if (buffer_.size() == 1 && buffer_[0] != 0xAA) {
        // show unsynced noise in chunks
        if (buffer_.size() >= 16) {
          ESP_LOGV(TAG, "Unsynced: %s", format_hex_pretty(buffer_).c_str());
          buffer_.clear();
        } else {
          // keep waiting for 0xAA
          if (buffer_[0] != 0xAA) buffer_.clear();
        }
        continue;
      }

      // Need at least header [AA, len]
      if (buffer_.size() >= 2 && buffer_[0] == 0xAA) {
        uint8_t len = buffer_[1];
        const size_t frame_total = static_cast<size_t>(len) + 2; // keep your protocol assumption

        if (buffer_.size() >= frame_total) {
          std::vector<uint8_t> frame(buffer_.begin(), buffer_.begin() + frame_total);

          if (frame_total >= 4) {
            uint16_t crc_calc = crc16(frame.data(), frame.size() - 2);
            uint16_t crc_rcv = static_cast<uint16_t>(frame[frame.size() - 2])
                            | (static_cast<uint16_t>(frame.back()) << 8);

            if (crc_calc == crc_rcv) {
              ESP_LOGD(TAG, "Valid frame: %s → %s",
                      format_hex_pretty(frame).c_str(),
                      decode_frame(frame).c_str());
            } else {
              ESP_LOGW(TAG, "CRC mismatch: %s (calc=0x%04X recv=0x%04X)",
                      format_hex_pretty(frame).c_str(), crc_calc, crc_rcv);
            }
          } else {
            ESP_LOGV(TAG, "Short frame: %s", format_hex_pretty(frame).c_str());
          }

          buffer_.erase(buffer_.begin(), buffer_.begin() + frame_total);
          // If next byte isn’t 0xAA, purge to resync
          if (!buffer_.empty() && buffer_[0] != 0xAA) buffer_.clear();
        } else if (buffer_.size() > 128) {
          ESP_LOGV(TAG, "Overflow guard, partial: %s", format_hex_pretty(buffer_).c_str());
          buffer_.clear();
        }
      }
    }
  }


 protected:
  // Modbus-like CRC16 (0xA001) used in your code
  static uint16_t crc16(const uint8_t *data, size_t size) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1)
          crc = (crc >> 1) ^ 0xA001;
        else
          crc >>= 1;
      }
    }
    return crc;
  }

  static std::string decode_frame(const std::vector<uint8_t> &frame) {
    // Your original decode logic, kept as-is
    if (frame.size() < 5) return "Invalid";
    uint8_t cmd = frame[4];
    char desc[64];
    if (cmd == 0x01) {
      const char* on_off = frame.size() > 5 && frame[5] ? "ON" : "OFF";
      const char* modes[] = {"?", "Cool", "Dry", "Fan", "Heat", "Auto"};
      uint8_t mode = (frame.size() > 6) ? (frame[6] % 6) : 0;
      snprintf(desc, sizeof(desc), "Power %s, Mode %s", on_off, modes[mode]);
    } else if (cmd == 0x02) {
      int temp = (frame.size() > 5) ? frame[5] : -1;
      snprintf(desc, sizeof(desc), "Set temp %d °C", temp);
    } else if (cmd == 0x03) {
      const char* fans[] = {"?", "Low", "Med", "High", "Auto"};
      uint8_t f = (frame.size() > 5) ? (frame[5] % 5) : 0;
      snprintf(desc, sizeof(desc), "Fan %s", fans[f]);
    } else if (cmd == 0x04) {
      bool on = (frame.size() > 5) ? (frame[5] != 0) : false;
      snprintf(desc, sizeof(desc), "Swing %s", on ? "ON" : "OFF");
    } else if (cmd == 0x10) {
      return "Status request";
    } else if (cmd == 0x11) {
      return "Status reply";
    } else {
      return "Unknown cmd";
    }
    return std::string(desc);
  }

  std::vector<uint8_t> buffer_;
};

}  // namespace arv_rs485_logger
}  // namespace esphome
