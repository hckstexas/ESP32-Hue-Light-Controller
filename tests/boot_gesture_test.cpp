#include <cassert>
#include <iostream>
#include "../include/ButtonInput.h"

struct Fixture {
  ButtonState button{0};
  BootGestureState gesture;
  int singles = 0, doubles = 0, starts = 0, ends = 0;
  Fixture() { tick(true, 250); }
  BootGesture tick(bool high, uint32_t now) {
    const auto edge = updateButtonInput(button, high, now, 30, 250);
    const auto result = button.armed
        ? updateBootGesture(gesture, edge, button, high, now)
        : BootGesture::None;
    singles += result == BootGesture::SingleTap;
    doubles += result == BootGesture::DoubleTap;
    starts += result == BootGesture::HoldStart;
    ends += result == BootGesture::HoldEnd;
    return result;
  }
  void firstTap() {
    tick(false, 300);
    tick(false, 330);
    tick(true, 400);
    assert(tick(true, 430) == BootGesture::None);
  }
};

int main() {
  Fixture single;
  single.firstTap();
  assert(single.tick(true, 779) == BootGesture::None);
  assert(single.tick(true, 780) == BootGesture::SingleTap);
  assert(single.tick(true, 2000) == BootGesture::None);
  assert(single.singles == 1 && single.doubles == 0 && single.starts == 0);

  Fixture twice;
  twice.firstTap();
  twice.tick(false, 500);
  twice.tick(false, 530);
  twice.tick(true, 600);
  assert(twice.tick(true, 630) == BootGesture::DoubleTap);
  twice.tick(true, 2000);
  assert(twice.singles == 0 && twice.doubles == 1 && twice.starts == 0);

  // A second short press begun inside the window can finish after it.
  Fixture lateRelease;
  lateRelease.firstTap();
  lateRelease.tick(false, 740);
  lateRelease.tick(false, 770);
  assert(lateRelease.tick(false, 1000) == BootGesture::None);
  lateRelease.tick(true, 1100);
  assert(lateRelease.tick(true, 1130) == BootGesture::DoubleTap);
  assert(lateRelease.singles == 0);

  Fixture hold;
  hold.tick(false, 300);
  hold.tick(false, 330);
  assert(hold.tick(false, 829) == BootGesture::None);
  assert(hold.tick(false, 830) == BootGesture::HoldStart);
  assert(hold.tick(false, 900) == BootGesture::None);
  hold.tick(true, 950);
  assert(hold.tick(true, 980) == BootGesture::HoldEnd);
  hold.tick(true, 2000);
  assert(hold.starts == 1 && hold.ends == 1 && hold.singles == 0 &&
         hold.doubles == 0);

  // Tap + hold dims only; it must not toggle or cycle first.
  Fixture tapHold;
  tapHold.firstTap();
  tapHold.tick(false, 500);
  tapHold.tick(false, 530);
  assert(tapHold.tick(false, 1030) == BootGesture::HoldStart);
  tapHold.tick(true, 1100);
  assert(tapHold.tick(true, 1130) == BootGesture::HoldEnd);
  tapHold.tick(true, 2000);
  assert(tapHold.singles == 0 && tapHold.doubles == 0);

  Fixture slow;
  slow.firstTap();
  slow.tick(true, 780);
  slow.tick(false, 900);
  slow.tick(false, 930);
  slow.tick(true, 1000);
  slow.tick(true, 1030);
  assert(slow.tick(true, 1380) == BootGesture::SingleTap);
  assert(slow.singles == 2 && slow.doubles == 0);

  // Contact bounce cannot become a second tap.
  Fixture bounce;
  bounce.firstTap();
  bounce.tick(false, 500);
  bounce.tick(true, 510);
  assert(bounce.tick(true, 780) == BootGesture::SingleTap);
  assert(bounce.doubles == 0);

  // Do not start a hold during release debounce, even if 500 ms has elapsed.
  Fixture releaseAtLimit;
  releaseAtLimit.tick(false, 300);
  releaseAtLimit.tick(false, 330);
  assert(releaseAtLimit.tick(true, 829) == BootGesture::None);
  assert(releaseAtLimit.tick(true, 830) == BootGesture::None);
  assert(releaseAtLimit.tick(true, 859) == BootGesture::None);
  releaseAtLimit.tick(true, 2000);
  assert(releaseAtLimit.starts == 0 && releaseAtLimit.singles == 0);

  // A wake-only press uses a cleared tracker, leaving no pending toggle.
  Fixture wake;
  wake.firstTap();
  wake.gesture = BootGestureState{};
  assert(wake.tick(true, 2000) == BootGesture::None);
  assert(wake.singles == 0 && wake.doubles == 0);

  BootGestureState rollover;
  rollover.tapPending = true;
  rollover.firstReleasedAt = UINT32_MAX - 100;
  ButtonState released(0);
  assert(updateBootGesture(rollover, ButtonEvent::None, released, true, 249) ==
         BootGesture::SingleTap);
  std::cout << "PASS: single/double tap separation, hold, tap+hold, "
               "late release, slow taps, bounce, wake reset, rollover\n";
}
