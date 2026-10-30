#include <cassert>
#include <iostream>
#include "../include/ButtonInput.h"

static ButtonEvent sample(ButtonState& b, bool high, uint32_t now) {
  return updateButtonInput(b, high, now, 30, 250);
}

int main() {
  // Power-up with a grounded input must never become a press or hold.
  ButtonState startupLow(0);
  for (uint32_t t = 0; t < 10000; t += 4) {
    assert(sample(startupLow, false, t) == ButtonEvent::None);
    assert(!startupLow.armed);
    assert(startupLow.stableHigh);
  }
  assert(sample(startupLow, true, 10000) == ButtonEvent::None);
  assert(sample(startupLow, true, 10250) == ButtonEvent::Armed);

  // Stable release is required; brief high pulses on an unused input don't arm.
  ButtonState noisy(35);
  for (uint32_t t = 0; t < 2000; t += 10) {
    assert(sample(noisy, t % 20 == 0, t) == ButtonEvent::None);
  }
  assert(!noisy.armed);

  ButtonState boot(0);
  assert(sample(boot, true, 0) == ButtonEvent::None);
  assert(sample(boot, true, 250) == ButtonEvent::Armed);
  assert(sample(boot, false, 300) == ButtonEvent::None);
  assert(sample(boot, true, 310) == ButtonEvent::None);  // contact bounce
  assert(sample(boot, false, 320) == ButtonEvent::None);
  assert(sample(boot, false, 349) == ButtonEvent::None);
  assert(sample(boot, false, 350) == ButtonEvent::Pressed);
  assert(boot.pressedAt == 350);
  for (uint32_t t = 354; t < 2000; t += 4) {
    assert(sample(boot, false, t) == ButtonEvent::None);  // no repeat presses
  }
  assert(sample(boot, true, 2000) == ButtonEvent::None);
  assert(sample(boot, true, 2030) == ButtonEvent::Released);
  assert(sample(boot, true, 2100) == ButtonEvent::None);

  // Filtering is independent for the two physical inputs.
  ButtonState room(35);
  assert(sample(room, true, 250) == ButtonEvent::Armed);
  assert(sample(room, false, 500) == ButtonEvent::None);
  assert(sample(room, false, 530) == ButtonEvent::Pressed);
  assert(boot.stableHigh && !room.stableHigh);
  assert(sample(room, true, 600) == ButtonEvent::None);
  assert(sample(room, true, 630) == ButtonEvent::Released);

  // Unsigned time subtraction must continue working after millis() wraps.
  ButtonState wrap(0);
  wrap.rawChangedAt = UINT32_MAX - 100;
  assert(sample(wrap, true, 200) == ButtonEvent::Armed);
  wrap.rawChangedAt = UINT32_MAX - 10;
  wrap.rawHigh = false;
  assert(sample(wrap, false, 20) == ButtonEvent::Pressed);
  std::cout << "PASS: startup, noisy input, debounce, hold, release, "
               "independent buttons, millis rollover\n";
}
