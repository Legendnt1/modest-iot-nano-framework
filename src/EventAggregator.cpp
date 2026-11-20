/**
 * @file EventAggregator.cpp
 * @brief Implementation of composite event pattern matching.
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-19
 * @version 1.0
 */

#include "EventAggregator.h"
#include "TimeSynchronizationDriver.h"

// --- Constructor ---

EventAggregator::EventAggregator()
    : capturedEvents{}, capturedEventCount(0),
      writeIndex(0),
      patternCount(0),
      outputHandler(nullptr) {
    for (auto &pattern: patterns) {
        pattern.active = false;
        pattern.identifierCount = 0;
    }
}

// --- Public Methods ---

bool EventAggregator::addPattern(const int* eventIdentifiers, const uint8_t eventIdentifierCount,
                                 const uint32_t timeWindowSeconds, const int compositeEventId) {
    if (patternCount >= EVENT_AGGREGATOR_MAX_PATTERNS) {
        return false;
    }
    if (eventIdentifierCount == 0 || eventIdentifierCount > EVENT_AGGREGATOR_MAX_PATTERN_SIZE) {
        return false;
    }

    EventPattern& pattern = patterns[patternCount];
    for (uint8_t i = 0; i < eventIdentifierCount; i++) {
        pattern.identifiers[i] = eventIdentifiers[i];
    }
    pattern.identifierCount = eventIdentifierCount;
    pattern.timeWindowSeconds = timeWindowSeconds;
    pattern.compositeEventId = compositeEventId;
    pattern.active = true;

    patternCount++;
    return true;
}

void EventAggregator::setOutputHandler(EventHandler* handler) {
    outputHandler = handler;
}

void EventAggregator::on(const Event event) {
    const uint32_t currentTimestamp = TimeSynchronizationDriver::getCurrentTimestamp();

    // Capture the event in the circular buffer
    captureEvent(event, currentTimestamp);

    // Check all patterns for a match
    checkForEventPatternMatch(currentTimestamp);

    // Remove expired captured events
    removeExpiredCapturedEvents(currentTimestamp);
}

// --- Private Helpers ---

// --- Pattern Matching ---

void EventAggregator::checkForEventPatternMatch(const uint32_t currentTimestamp) const {
    if (outputHandler == nullptr || patternCount == 0) {
        return;
    }

    for (uint8_t p = 0; p < patternCount; p++) {
        const EventPattern& pattern = patterns[p];
        if (!pattern.active) {
            continue;
        }

        // Check if all identifiers in the pattern are present within the time window
        bool allFound = true;
        uint32_t oldestInPattern = currentTimestamp;

        for (uint8_t i = 0; i < pattern.identifierCount; i++) {
            const int targetId = pattern.identifiers[i];
            bool found = false;

            // Search captured events for this identifier within the time window
            for (uint8_t eventOffset = 0; eventOffset < capturedEventCount; eventOffset++) {
                const uint8_t idx = getCapturedEventIndex(eventOffset);
                const CapturedEvent& capturedEvent = capturedEvents[idx];

                // Check if this captured event is within the time window
                if (currentTimestamp - capturedEvent.timestamp > pattern.timeWindowSeconds) {
                    continue;
                }

                if (capturedEvent.identifier == targetId) {
                    found = true;
                    if (capturedEvent.timestamp < oldestInPattern) {
                        oldestInPattern = capturedEvent.timestamp;
                    }
                    break;
                }
            }

            if (!found) {
                allFound = false;
                break;
            }
        }

        // If all identifiers found within the time window, emit composite event
        if (allFound) {
            // Create and emit the composite event
            const Event compositeEvent(pattern.compositeEventId);
            outputHandler->on(compositeEvent);
        }
    }
}

// --- Captured Event Cleanup ---

void EventAggregator::removeExpiredCapturedEvents(const uint32_t currentTimestamp) {
    const uint32_t maxWindow = getMaxTimeWindow();
    if (maxWindow == 0 || capturedEventCount == 0) {
        return;
    }

    // Remove captured events older than the maximum time window
    uint8_t newCapturedEventCount = 0;
    for (uint8_t eventOffset = 0; eventOffset < capturedEventCount; eventOffset++) {
        const uint8_t idx = getCapturedEventIndex(eventOffset);

        if (const CapturedEvent& capturedEvent = capturedEvents[idx]; currentTimestamp - capturedEvent.timestamp <= maxWindow) {
            // Keep this event: compact it to the front
            if (newCapturedEventCount != eventOffset) {
                const uint8_t destIdx = getCapturedEventIndex(newCapturedEventCount);
                capturedEvents[destIdx] = capturedEvent;
            }
            newCapturedEventCount++;
        }
    }

    capturedEventCount = newCapturedEventCount;
}

uint32_t EventAggregator::getMaxTimeWindow() const {
    uint32_t maxWindow = 0;
    for (uint8_t p = 0; p < patternCount; p++) {
        if (patterns[p].active && patterns[p].timeWindowSeconds > maxWindow) {
            maxWindow = patterns[p].timeWindowSeconds;
        }
    }
    return maxWindow;
}

void EventAggregator::captureEvent(const Event event, const uint32_t timestamp) {
    capturedEvents[writeIndex].identifier = event.identifier;
    capturedEvents[writeIndex].sourceId = event.sourceId;
    capturedEvents[writeIndex].timestamp = timestamp;
    writeIndex = (writeIndex + 1) % EVENT_AGGREGATOR_MAX_CAPTURED_EVENTS;
    if (capturedEventCount < EVENT_AGGREGATOR_MAX_CAPTURED_EVENTS) {
        capturedEventCount++;
    }
}

uint8_t EventAggregator::getCapturedEventIndex(const uint8_t offsetFromOldest) const {
    return static_cast<uint8_t>((writeIndex - capturedEventCount + offsetFromOldest + EVENT_AGGREGATOR_MAX_CAPTURED_EVENTS) % EVENT_AGGREGATOR_MAX_CAPTURED_EVENTS);
}
