/**
 * @file test.cpp
 * @brief Host tests for EventAggregator.
 * @date 2026-11-19
 * @version 1.0
 */

#include <cstdio>
#include <cassert>

// Minimal millis() mock for this test
static unsigned long testFakeMillis = 0;
extern "C" unsigned long millis() { return testFakeMillis; }

#include "EventAggregator.h"
#include "TimeSynchronizationDriver.h"

// --- Test Helpers ---

/**
 * @brief Mock output handler that captures composite events.
 */
class MockOutputHandler : public EventHandler {
public:
    int lastEventId = -1;
    int eventCount = 0;

    void on(Event event) override {
        lastEventId = event.identifier;
        eventCount++;
    }

    void reset() {
        lastEventId = -1;
        eventCount = 0;
    }
};

/**
 * @brief Advances the fake clock by the given seconds.
 */
void advanceTime(uint32_t seconds) {
    testFakeMillis += static_cast<unsigned long>(seconds) * 1000;
}

// --- Tests ---

void test_single_event_pattern() {
    std::printf("  Test 1: Single event pattern...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern: event 30 → composite 400
    int ids[] = {30};
    aggregator.addPattern(ids, 1, 5, 400);

    // Fire event 30
    aggregator.on(Event(30));

    assert(handler.eventCount == 1);
    assert(handler.lastEventId == 400);
    std::printf("    Single event pattern: PASS\n");
}

void test_two_event_and_pattern_within_window() {
    std::printf("  Test 2: Two-event AND pattern within time window...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern: motion(30) AND door(72) within 5s → composite 400
    int ids[] = {30, 72};
    aggregator.addPattern(ids, 2, 5, 400);

    // Fire motion at t=0
    aggregator.on(Event(30));
    assert(handler.eventCount == 0); // door not yet seen

    // Advance 2 seconds, fire door
    advanceTime(2);
    aggregator.on(Event(72));

    assert(handler.eventCount == 1);
    assert(handler.lastEventId == 400);
    std::printf("    Two-event AND within window: PASS\n");
}

void test_two_event_and_pattern_outside_window() {
    std::printf("  Test 3: Two-event AND pattern outside time window...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern: motion(30) AND door(72) within 5s → composite 400
    int ids[] = {30, 72};
    aggregator.addPattern(ids, 2, 5, 400);

    // Fire motion at t=0
    aggregator.on(Event(30));

    // Advance 10 seconds (outside 5s window), fire door
    advanceTime(10);
    aggregator.on(Event(72));

    assert(handler.eventCount == 0);
    std::printf("    Two-event AND outside window: PASS\n");
}

void test_three_event_pattern() {
    std::printf("  Test 4: Three-event pattern...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern: motion(30) AND door(72) AND leak(70) within 10s → composite 401
    int ids[] = {30, 72, 70};
    aggregator.addPattern(ids, 3, 10, 401);

    // Fire motion at t=0
    aggregator.on(Event(30));
    assert(handler.eventCount == 0);

    // Fire door at t=3
    advanceTime(3);
    aggregator.on(Event(72));
    assert(handler.eventCount == 0);

    // Fire leak at t=5 (within 10s of motion)
    advanceTime(2);
    aggregator.on(Event(70));

    assert(handler.eventCount == 1);
    assert(handler.lastEventId == 401);
    std::printf("    Three-event pattern: PASS\n");
}

void test_multiple_patterns_registered() {
    std::printf("  Test 5: Multiple patterns registered...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern 1: motion(30) AND door(72) within 5s → 400
    int ids1[] = {30, 72};
    aggregator.addPattern(ids1, 2, 5, 400);

    // Pattern 2: leak(70) AND gas(40) within 10s → 401
    int ids2[] = {70, 40};
    aggregator.addPattern(ids2, 2, 10, 401);

    // Fire motion at t=0
    aggregator.on(Event(30));

    // Fire door at t=2 → pattern 1 matches
    advanceTime(2);
    aggregator.on(Event(72));
    assert(handler.eventCount == 1);
    assert(handler.lastEventId == 400);

    // Fire leak at t=7 (motion at t=0 is now outside 5s window)
    advanceTime(5);
    aggregator.on(Event(70));
    assert(handler.eventCount == 1); // gas not yet seen

    // Fire gas at t=12 → pattern 2 matches (leak at t=7, gas at t=12, within 10s)
    advanceTime(5);
    aggregator.on(Event(40));
    assert(handler.eventCount == 2);
    assert(handler.lastEventId == 401);
    std::printf("    Multiple patterns: PASS\n");
}

void test_captured_event_cleanup() {
    std::printf("  Test 6: Captured event cleanup...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern: motion(30) within 5s → 400
    int ids[] = {30};
    aggregator.addPattern(ids, 1, 5, 400);

    // Fire motion at t=0
    aggregator.on(Event(30));
    assert(handler.eventCount == 1);

    // Advance 10 seconds (outside 5s window)
    advanceTime(10);

    // Fire motion again — the old captured event should be cleaned up,
    // but this new event should still match (single event pattern)
    aggregator.on(Event(30));
    assert(handler.eventCount == 2);
    std::printf("    Captured event cleanup: PASS\n");
}

void test_circular_buffer_overflow() {
    std::printf("  Test 7: Circular buffer overflow...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // Pattern: event 99 within 100s → 400
    int ids[] = {99};
    aggregator.addPattern(ids, 1, 100, 400);

    // Fill the buffer with 33 events (1 more than max 32)
    for (int i = 0; i < 33; i++) {
        advanceTime(1);
        aggregator.on(Event(99));
    }

    // The aggregator should not crash and should still process events
    assert(handler.eventCount == 33);
    std::printf("    Circular buffer overflow: PASS\n");
}

void test_no_patterns_registered() {
    std::printf("  Test 8: No patterns registered...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    MockOutputHandler handler;
    aggregator.setOutputHandler(&handler);

    // No patterns — events should be captured but no composite emitted
    aggregator.on(Event(30));
    aggregator.on(Event(72));

    assert(handler.eventCount == 0);
    std::printf("    No patterns registered: PASS\n");
}

void test_pattern_array_full() {
    std::printf("  Test 9: Pattern array full...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;

    // Fill all 4 pattern slots
    int ids1[] = {30};
    int ids2[] = {72};
    int ids3[] = {70};
    int ids4[] = {40};
    assert(aggregator.addPattern(ids1, 1, 5, 400) == true);
    assert(aggregator.addPattern(ids2, 1, 5, 401) == true);
    assert(aggregator.addPattern(ids3, 1, 5, 402) == true);
    assert(aggregator.addPattern(ids4, 1, 5, 403) == true);

    // 5th pattern should fail
    int ids5[] = {99};
    assert(aggregator.addPattern(ids5, 1, 5, 404) == false);
    std::printf("    Pattern array full: PASS\n");
}

void test_invalid_pattern_parameters() {
    std::printf("  Test 10: Invalid pattern parameters...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;

    // Zero identifiers should fail
    int empty[] = {};
    assert(aggregator.addPattern(empty, 0, 5, 400) == false);

    // Too many identifiers should fail
    int tooMany[] = {1, 2, 3, 4, 5};
    assert(aggregator.addPattern(tooMany, 5, 5, 400) == false);

    // Valid pattern should succeed
    int valid[] = {30};
    assert(aggregator.addPattern(valid, 1, 5, 400) == true);
    std::printf("    Invalid pattern parameters: PASS\n");
}

void test_no_output_handler() {
    std::printf("  Test 11: No output handler set...\n");
    testFakeMillis = 0;

    EventAggregator aggregator;
    // No output handler set

    int ids[] = {30};
    aggregator.addPattern(ids, 1, 5, 400);

    // Should not crash even without output handler
    aggregator.on(Event(30));
    std::printf("    No output handler: PASS\n");
}

// --- Main ---

int main() {
    std::printf("Running EventAggregator tests...\n");

    test_single_event_pattern();
    test_two_event_and_pattern_within_window();
    test_two_event_and_pattern_outside_window();
    test_three_event_pattern();
    test_multiple_patterns_registered();
    test_captured_event_cleanup();
    test_circular_buffer_overflow();
    test_no_patterns_registered();
    test_pattern_array_full();
    test_invalid_pattern_parameters();
    test_no_output_handler();

    std::printf("All EventAggregator tests passed.\n");
    return 0;
}
