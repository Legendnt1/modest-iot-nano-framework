#include "DiagnosticsTelemetryPackage.h"
#include "Device.h"

// --- Constructor ---

DiagnosticsTelemetryPackage::DiagnosticsTelemetryPackage(const struct DiagnosticsSnapshot& snapshot)
    : missedTicks(snapshot.missedTicks),
      worstCaseWakeMicros(snapshot.worstCaseWakeMicros),
      packetsEnqueued(snapshot.packetsEnqueued),
      packetsDropped(snapshot.packetsDropped),
      packetsDroppedOldest(snapshot.packetsDroppedOldest),
      queueHighWaterMark(snapshot.queueHighWaterMark),
      highPriorityEnqueued(snapshot.highPriorityEnqueued),
      highPriorityDropped(snapshot.highPriorityDropped),
      highPriorityQueueHighWaterMark(snapshot.highPriorityQueueHighWaterMark),
      sendsSucceeded(snapshot.sendsSucceeded),
      sendsFailed(snapshot.sendsFailed),
      freeHeapBytes(snapshot.freeHeapBytes),
      minFreeHeapBytes(snapshot.minFreeHeapBytes),
      samplerStackFreeWords(snapshot.samplerStackFreeWords),
      networkStackFreeWords(snapshot.networkStackFreeWords),
      uptimeSeconds(snapshot.uptimeSeconds)
{
}

// --- TelemetryPackage Implementation ---

void DiagnosticsTelemetryPackage::write(TelemetryWriter& writer) const {
    writer.field(JSON_KEY_MISSED_TICKS, static_cast<int32_t>(missedTicks));
    writer.field(JSON_KEY_WORST_CASE_WAKE_MICROS, static_cast<int32_t>(worstCaseWakeMicros));
    writer.field(JSON_KEY_PACKETS_ENQUEUED, static_cast<int32_t>(packetsEnqueued));
    writer.field(JSON_KEY_PACKETS_DROPPED, static_cast<int32_t>(packetsDropped));
    writer.field(JSON_KEY_PACKETS_DROPPED_OLDEST, static_cast<int32_t>(packetsDroppedOldest));
    writer.field(JSON_KEY_QUEUE_HIGH_WATER_MARK, static_cast<int32_t>(queueHighWaterMark));
    writer.field(JSON_KEY_HIGH_PRIORITY_ENQUEUED, static_cast<int32_t>(highPriorityEnqueued));
    writer.field(JSON_KEY_HIGH_PRIORITY_DROPPED, static_cast<int32_t>(highPriorityDropped));
    writer.field(JSON_KEY_HIGH_PRIORITY_Q_HWM, static_cast<int32_t>(highPriorityQueueHighWaterMark));
    writer.field(JSON_KEY_SENDS_SUCCEEDED, static_cast<int32_t>(sendsSucceeded));
    writer.field(JSON_KEY_SENDS_FAILED, static_cast<int32_t>(sendsFailed));
    writer.field(JSON_KEY_FREE_HEAP_BYTES, static_cast<int32_t>(freeHeapBytes));
    writer.field(JSON_KEY_MIN_FREE_HEAP_BYTES, static_cast<int32_t>(minFreeHeapBytes));
    writer.field(JSON_KEY_SAMPLER_STACK_FREE, static_cast<int32_t>(samplerStackFreeWords));
    writer.field(JSON_KEY_NETWORK_STACK_FREE, static_cast<int32_t>(networkStackFreeWords));
    writer.field(JSON_KEY_UPTIME_SECONDS, static_cast<int32_t>(uptimeSeconds));
}
