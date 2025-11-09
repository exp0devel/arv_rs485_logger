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
    static std::vector<uint8_t> buf;
    static uint32_t last = 0;
    const uint32_t now = micros();

    if (!buf.empty() && (now - last) > 5000) {
      ESP_LOGD("sniff", "Burst %u bytes: %s",
              (unsigned)buf.size(), format_hex_pretty(buf).c_str());
      buf.clear();
    }

    while (this->available()) {
      uint8_t b;
      this->read_byte(&b);
      buf.push_back(b);
      last = micros();
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
