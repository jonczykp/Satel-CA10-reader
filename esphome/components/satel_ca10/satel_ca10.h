#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <utility>
#include <vector>

namespace esphome {
namespace satel_ca10 {

static const uint8_t FRAME_SIZE = 32;

class SatelCA10Hub : public Component {
 public:
  void set_clk_pin(InternalGPIOPin *pin) { this->clk_pin_ = pin; }
  void set_data_pin(InternalGPIOPin *pin) { this->data_pin_ = pin; }
  void register_bit_sensor(uint8_t bit, binary_sensor::BinarySensor *sensor) {
    this->sensors_.emplace_back(bit, sensor);
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  static void gpio_intr(SatelCA10Hub *arg);

  InternalGPIOPin *clk_pin_{nullptr};
  InternalGPIOPin *data_pin_{nullptr};
  ISRInternalGPIOPin isr_clk_pin_;
  ISRInternalGPIOPin isr_data_pin_;

  std::vector<std::pair<uint8_t, binary_sensor::BinarySensor *>> sensors_;

  // written by the ISR
  volatile bool data_result_[FRAME_SIZE]{};
  volatile bool clk_state_{false};
  volatile bool prev_clk_state_{false};
  volatile uint32_t last_interrupt_time_{0};
  volatile bool data_ready_{false};
  volatile uint32_t bit_count_{FRAME_SIZE + 10};

  bool published_[FRAME_SIZE]{};
  bool first_frame_{true};
};

}  // namespace satel_ca10
}  // namespace esphome
