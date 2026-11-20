/**
 * @file EventAggregator.h
 * @brief Composite event pattern matching with time-windowed detection.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-19
 * @version 1.0
 *
 * The EventAggregator listens for events and matches composite patterns
 * (e.g., "motion AND door opened within 5 seconds"). When a pattern matches,
 * it emits a composite event that can trigger automation rules.
 *
 * All storage uses fixed-size arrays to avoid heap allocation.
 */

#ifndef MODEST_IOT_NANO_FRAMEWORK_EVENT_AGGREGATOR_H
#define MODEST_IOT_NANO_FRAMEWORK_EVENT_AGGREGATOR_H

#include <cstdint>

#include "Event.h"
#include "EventHandler.h"

/**
 * @brief Maximum number of captured events in the circular buffer.
 * @note 32 captured events is sufficient for typical use cases (4 sensors × 8 events).
 */
static constexpr uint8_t EVENT_AGGREGATOR_MAX_CAPTURED_EVENTS = 32;

/**
 * @brief Maximum number of composite event patterns.
 * @note 4 patterns covers typical automation rules (motion+door, leak+door, etc.).
 */
static constexpr uint8_t EVENT_AGGREGATOR_MAX_PATTERNS = 4;

/**
 * @brief Maximum event identifiers per pattern.
 * @note 4 identifiers covers most composite patterns (e.g., motion + door + leak + gas).
 */
static constexpr uint8_t EVENT_AGGREGATOR_MAX_PATTERN_SIZE = 4;

/**
 * @brief Single captured event stored in the circular buffer.
 * @ingroup core_module
 */
struct CapturedEvent {
    int identifier; ///< Event identifier.
    int sourceId; ///< Event source identifier.
    uint32_t timestamp; ///< Absolute timestamp in seconds from TimeSynchronizationDriver.
};

/**
 * @brief Composite event pattern definition.
 * @ingroup core_module
 *
 * A pattern matches when all required event identifiers occur within the
 * specified time window. When matched, a composite event is emitted.
 */
struct EventPattern {
    int identifiers[EVENT_AGGREGATOR_MAX_PATTERN_SIZE]; ///< Event identifiers to match.
    uint8_t identifierCount; ///< Number of identifiers in this pattern.
    uint32_t timeWindowSeconds; ///< Maximum seconds between first and last event.
    int compositeEventId; ///< Event identifier emitted when the pattern matches.
    bool active; ///< True if this pattern slot is registered.
};

/**
 * @brief Matches composite event patterns with time-windowed detection.
 * @ingroup core_module
 *
 * Implements EventHandler to intercept events from sensors. Captures each event
 * with its timestamp, then checks all registered patterns. When a pattern
 * matches (all required events occurred within the time window), emits a
 * composite event via the output handler.
 *
 * @par Zero Heap Allocation
 * Uses fixed-size circular buffer (32 captured events) and pattern array (4 patterns).
 * No dynamic allocation.
 */
class EventAggregator : public EventHandler {
public:
    /**
     * @brief Constructs an EventAggregator with no patterns registered.
     */
    EventAggregator();

    /**
     * @brief Registers a composite event pattern.
     * @param eventIdentifiers Array of event identifiers that must all occur.
     * @param eventIdentifierCount Number of identifiers in the array.
     * @param timeWindowSeconds Maximum seconds between the first and last
     *        event in the pattern. All events must occur within this window.
     * @param compositeEventId Event identifier emitted when the pattern matches.
     * @return True if the pattern was registered, false if the pattern array is full.
     * @note Patterns match AND logic: all identifiers must occur within the window.
     */
    bool addPattern(const int* eventIdentifiers, uint8_t eventIdentifierCount,
                    uint32_t timeWindowSeconds, int compositeEventId);

    /**
     * @brief Sets the output handler for composite events.
     * @param handler The EventHandler that receives composite events when patterns match.
     * @note Typically the Device instance, which dispatches to the developer's on(Event) override.
     */
    void setOutputHandler(EventHandler* handler);

    /**
     * @brief Handles incoming events from sensors.
     * @param event The event to capture and check against patterns.
     * @note Implements EventHandler::on(). Captures the event with its timestamp,
     *       checks all registered patterns, and emits composite events when matched.
     */
    void on(Event event) override;

private:
    /** @brief Captured events in a circular buffer. */
    CapturedEvent capturedEvents[EVENT_AGGREGATOR_MAX_CAPTURED_EVENTS];

    /** @brief Number of captured events currently in the buffer. */
    uint8_t capturedEventCount;

    /** @brief Index of the next write position in the circular buffer. */
    uint8_t writeIndex;

    /** @brief Registered composite event patterns. */
    EventPattern patterns[EVENT_AGGREGATOR_MAX_PATTERNS];

    /** @brief Number of active patterns. */
    uint8_t patternCount;

    /** @brief Output handler for composite events. */
    EventHandler* outputHandler;

    /**
     * @brief Checks all registered patterns for a match.
     * @param currentTimestamp The timestamp of the most recent event.
     * @note Iterates all active patterns. If a pattern matches, emits the
     *       composite event via the output handler.
     */
    void checkForEventPatternMatch(const uint32_t currentTimestamp) const;

    /**
     * @brief Removes captured events older than the maximum pattern time window.
     * @param currentTimestamp The current timestamp.
     * @note Prevents the circular buffer from filling with stale events.
     */
    void removeExpiredCapturedEvents(const uint32_t currentTimestamp);

    /**
     * @brief Captures an event in the circular buffer.
     * @param event The event to capture.
     * @param timestamp The current timestamp.
     * @note Handles both buffer-not-full and buffer-full (circular overwrite) cases.
     */
    void captureEvent(const Event event, const uint32_t timestamp);

    /**
     * @brief Returns the circular buffer index for a given offset from the oldest captured event.
     * @param offsetFromOldest The offset from the oldest captured event (0 = oldest).
     * @return The index into the capturedEvents array.
     * @note Encapsulates the circular buffer arithmetic to avoid formula duplication.
     */
    uint8_t getCapturedEventIndex(const uint8_t offsetFromOldest) const;

    /**
     * @brief Returns the maximum time window across all active patterns.
     * @return The largest timeWindowSeconds value, or 0 if no patterns are active.
     */
    uint32_t getMaxTimeWindow() const;
};

#endif // MODEST_IOT_NANO_FRAMEWORK_EVENT_AGGREGATOR_H
