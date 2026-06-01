#include <Arduino.h>
#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;

// ----- User configuration -----
// Change this value to configure how many pixels each movement tick sends.
constexpr int8_t MOVE_PIXELS = 8;

// Movement repeat rate while a direction input remains high.
constexpr uint16_t MOVE_REPEAT_MS = 20;

// Button debounce for the click input.
constexpr uint16_t DEBOUNCE_MS = 25;

// false is the default for common-ground switches/joysticks:
// GPIO input is HIGH at rest and becomes active when connected to GND.
constexpr bool INPUT_ACTIVE_HIGH = false;

constexpr uint8_t PIN_UP = 1;
constexpr uint8_t PIN_DOWN = 2;
constexpr uint8_t PIN_LEFT = 3;
constexpr uint8_t PIN_RIGHT = 4;
constexpr uint8_t PIN_CLICK = 5;
// ------------------------------

struct DebouncedInput {
  uint8_t pin;
  bool stableActive;
  bool lastRawActive;
  uint32_t lastRawChangeMs;
};

DebouncedInput clickInput{PIN_CLICK, false, false, 0};
uint32_t lastMoveMs = 0;

bool readActive(uint8_t pin) {
  const int level = digitalRead(pin);
  return INPUT_ACTIVE_HIGH ? level == HIGH : level == LOW;
}

void configureInput(uint8_t pin) {
  pinMode(pin, INPUT_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
}

bool updateDebounced(DebouncedInput &input) {
  const bool rawActive = readActive(input.pin);
  const uint32_t now = millis();

  if (rawActive != input.lastRawActive) {
    input.lastRawActive = rawActive;
    input.lastRawChangeMs = now;
  }

  if ((now - input.lastRawChangeMs) >= DEBOUNCE_MS &&
      input.stableActive != rawActive) {
    input.stableActive = rawActive;
    return true;
  }

  return false;
}

void setup() {
  configureInput(PIN_UP);
  configureInput(PIN_DOWN);
  configureInput(PIN_LEFT);
  configureInput(PIN_RIGHT);
  configureInput(PIN_CLICK);

  Mouse.begin();
  USB.begin();

  clickInput.lastRawActive = readActive(clickInput.pin);
  clickInput.stableActive = clickInput.lastRawActive;
  clickInput.lastRawChangeMs = millis();
}

void loop() {
  const uint32_t now = millis();

  if ((now - lastMoveMs) >= MOVE_REPEAT_MS) {
    int8_t deltaX = 0;
    int8_t deltaY = 0;

    if (readActive(PIN_LEFT)) {
      deltaX -= MOVE_PIXELS;
    }
    if (readActive(PIN_RIGHT)) {
      deltaX += MOVE_PIXELS;
    }
    if (readActive(PIN_UP)) {
      deltaY -= MOVE_PIXELS;
    }
    if (readActive(PIN_DOWN)) {
      deltaY += MOVE_PIXELS;
    }

    if (deltaX != 0 || deltaY != 0) {
      Mouse.move(deltaX, deltaY, 0);
    }

    lastMoveMs = now;
  }

  if (updateDebounced(clickInput)) {
    if (clickInput.stableActive) {
      Mouse.press(MOUSE_LEFT);
    } else {
      Mouse.release(MOUSE_LEFT);
    }
  }
}
