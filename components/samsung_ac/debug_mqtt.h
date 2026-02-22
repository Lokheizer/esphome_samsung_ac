#pragma once
#include <string>

namespace esphome {
namespace samsung_ac {

inline bool debug_mqtt_connected() { return false; }

inline void debug_mqtt_connect(const std::string&, uint16_t,
                               const std::string&, const std::string&) {}

inline void debug_mqtt_publish(const std::string&, const std::string&) {}

}  // namespace samsung_ac
}  // namespace esphome
