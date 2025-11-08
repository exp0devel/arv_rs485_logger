#include "esphome.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

class ArvRs485Logger : public Component, public UARTDevice {
 public:
  ArvRs485Logger(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override {}

  void loop() override {
    std::vector<uint8_t> buffer;
    while (available()) {
      uint8_t byte;
      read_byte(&byte);
      buffer.push_back(byte);
      if (buffer.size() >= 4 && buffer[0] == 0xAA) {
        uint8_t len = buffer[1];
        if (buffer.size() >= len + 2) {
          std::vector<uint8_t> frame(buffer.begin(), buffer.begin() + len + 2);
          uint16_t crc_calc = crc16(frame.data(), frame.size() - 2);
          uint16_t crc_rcv = (frame.back() << 8) | frame[frame.size() - 2];
          if (crc_calc == crc_rcv) {
            ESP_LOGD("arv_rs485_logger", "Valid frame: %s → %s", format_hex_pretty(frame).c_str(), decode_frame(frame).c_str());
          }
          buffer.erase(buffer.begin(), buffer.begin() + len + 2);
        }
      } else if (buffer.size() > 50) {
        buffer.clear();
      }
    }
  }

  uint16_t crc16(const uint8_t *data, size_t size) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
      }
    }
    return crc;
  }

  std::string decode_frame(const std::vector<uint8_t> &frame) {
    if (frame.size() < 5) return "Invalid";
    uint8_t cmd = frame[4];
    char desc[64];
    if (cmd == 0x01) {
      const char* on_off = frame[5] ? "ON" : "OFF";
      const char* modes[] = {"?", "Cool", "Dry", "Fan", "Heat", "Auto"};
      sprintf(desc, "Power %s, Mode %s", on_off, modes[frame[6] % 6]);
    } else if (cmd == 0x02) {
      sprintf(desc, "Set temp %d °C", frame[5]);
    } else if (cmd == 0x03) {
      const char* fans[] = {"?", "Low", "Med", "High", "Auto"};
      sprintf(desc, "Fan %s", fans[frame[5] % 5]);
    } else if (cmd == 0x04) {
      sprintf(desc, "Swing %s", frame[5] ? "ON" : "OFF");
    } else if (cmd == 0x10) {
      return "Status request";
    } else if (cmd == 0x11) {
      return "Status reply";
    } else {
      return "Unknown cmd";
    }
    return std::string(desc);
  }
};