#include <time.h>
#include <cassert>
#include <Arduino.h>
#include "Led.h"

namespace {
struct Recorder : CommandHandler {
    int count = 0;
    void handle(const Command) override { count++; }
};
} // namespace

[[nodiscard]] int main() {
    // Test 1: Constructor initializes LED state
    {
        Recorder parent;
        Led led(5, false, &parent);
        assert(!led.isActive());
        assert(fakeDigitalWrite[5] == LOW);
        std::printf("  [PASS] Constructor initializes LED off\n");
    }

    // Test 2: Constructor with initial state on
    {
        Led led(6, true);
        assert(led.isActive());
        assert(fakeDigitalWrite[6] == HIGH);
        std::printf("  [PASS] Constructor initializes LED on\n");
    }

    // Test 3: TURN_ON_COMMAND turns LED on
    {
        Led led(7, false);
        assert(!led.isActive());
        led.handle(Led::TURN_ON_COMMAND);
        assert(led.isActive());
        assert(fakeDigitalWrite[7] == HIGH);
        std::printf("  [PASS] TURN_ON_COMMAND turns LED on\n");
    }

    // Test 4: TURN_OFF_COMMAND turns LED off
    {
        Led led(8, true);
        assert(led.isActive());
        led.handle(Led::TURN_OFF_COMMAND);
        assert(!led.isActive());
        assert(fakeDigitalWrite[8] == LOW);
        std::printf("  [PASS] TURN_OFF_COMMAND turns LED off\n");
    }

    // Test 5: TOGGLE_LED_COMMAND toggles state
    {
        Led led(9, false);
        assert(!led.isActive());
        led.handle(Led::TOGGLE_LED_COMMAND);
        assert(led.isActive());
        assert(fakeDigitalWrite[9] == HIGH);
        led.handle(Led::TOGGLE_LED_COMMAND);
        assert(!led.isActive());
        assert(fakeDigitalWrite[9] == LOW);
        std::printf("  [PASS] TOGGLE_LED_COMMAND toggles state\n");
    }

    // Test 6: Command propagation to parent handler
    {
        Recorder parent;
        Led led(10, false, &parent);
        led.handle(Led::TURN_ON_COMMAND);
        led.handle(Led::TOGGLE_LED_COMMAND);
        led.handle(Led::TURN_OFF_COMMAND);
        assert(parent.count == 3);
        std::printf("  [PASS] Command propagation to parent handler\n");
    }

    // Test 7: Unknown command does not change state
    {
        Led led(11, true);
        led.handle(Command{999});
        assert(led.isActive()); // State unchanged
        std::printf("  [PASS] Unknown command does not change state\n");
    }

    return 0;
}
