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
    while (this->available()) {
      uint8_t byte;
      if (!this->read_byte(&byte))
        break;

      buffer_.push_back(byte);

      // try to resync to 0xAA start
      if (buffer_.size() == 1 && buffer_[0] != 0xAA) {
        buffer_.clear();
        continue;
      }

      // We need at least header [0xAA, len]
      if (buffer_.size() >= 2 && buffer_[0] == 0xAA) {
        uint8_t len = buffer_[1];
        // frame = 0xAA + len + payload(len bytes?) + CRC_L + CRC_H
        // You wrote "len + 2" earlier; commonly it's 2(header)+len+2(CRC)=len+4 total.
        // If your proto really is len+2 total, keep that. Otherwise, adjust here.
        // Based on your original code, we'll keep len+2 total bytes after 0xAA.
        // That means total frame size = len + 2.
        const size_t frame_total = static_cast<size_t>(len) + 2;

        if (buffer_.size() >= frame_total) {
          std::vector<uint8_t> frame(buffer_.begin(), buffer_.begin() + frame_total);

          if (frame_total >= 4) {
            // last two bytes are CRC_L, CRC_H per your code
            uint16_t crc_calc = crc16(frame.data(), frame.size() - 2);
            uint16_t crc_rcv = static_cast<uint16_t>(frame[frame.size() - 2])
                             | (static_cast<uint16_t>(frame.back()) << 8);

            if (crc_calc == crc_rcv) {
              // Pretty hex plus decoded text
              std::string hex = format_hex_pretty(frame);
              std::string desc = decode_frame(frame);
              ESP_LOGD(TAG, "Valid frame: %s → %s", hex.c_str(), desc.c_str());
            } else {
              ESP_LOGV(TAG, "CRC mismatch (calc=0x%04X recv=0x%04X)", crc_calc, crc_rcv);
            }
          }

          // drop the processed frame and keep scanning
          buffer_.erase(buffer_.begin(), buffer_.begin() + frame_total);
          // If next byte isn't 0xAA, try to resync
          if (!buffer_.empty() && buffer_[0] != 0xAA) {
            buffer_.clear();
          }
        } else {
          // not enough bytes for a complete frame yet
          if (buffer_.size() > 128)  // simple overflow guard
            buffer_.clear();
        }
      } else if (buffer_.size() > 128) {
        buffer_.clear();
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
