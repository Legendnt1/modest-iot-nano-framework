/**
 * @file MqttGatewayClient.cpp
 * @brief Implements MQTT broker session handling and telemetry publish operations.
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-06-06
 * @version 1.0
 */

#include "MqttGatewayClient.h"
#include "ModestLog.h"

// Static pointer for callback access (set by constructor)
static MqttGatewayClient *gActiveMqttClient = nullptr;

/**
 * @brief Native JSON integer field extractor.
 *
 * Searches for "key": in the JSON string and extracts the following integer value.
 * This replaces ArduinoJson's deserializeJson for the simple 2-field MQTT command format.
 *
 * @param jsonString Null-terminated JSON string.
 * @param key The field name to search for.
 * @param outValue Pointer to store the extracted value.
 * @return True if the field was found and parsed successfully.
 */
static bool parseJsonInt(const char *jsonString, const char *key, int *outValue) {
    constexpr auto pattern = "\"";
    const char *pos = strstr(jsonString, pattern);
    while (pos != nullptr) {
        pos = pos + 1; // Skip opening quote
        const char *keyStart = pos;
        const char *keyEnd = strchr(pos, '"');
        if (keyEnd == nullptr) break;

        if (const size_t keyLen = static_cast<size_t>(keyEnd - keyStart);
            strlen(key) == keyLen && strncmp(keyStart, key, keyLen) == 0) {
            // Found the key, now find the value after ": "
            const char *valueStart = keyEnd + 1;
            while (*valueStart == ' ' || *valueStart == ':') {
                valueStart++;
            }
            char *endPtr;
            const long val = strtol(valueStart, &endPtr, 10);
            if (endPtr != valueStart) {
                *outValue = static_cast<int>(val);
                return true;
            }
        }
        // Look for the next key
        pos = strchr(keyEnd, '"');
    }
    return false;
}

/**
 * @brief Native JSON int32_t field extractor.
 *
 * @param json Null-terminated JSON string.
 * @param key The field name to search for.
 * @param outValue Pointer to store the extracted value.
 * @return True if the field was found and parsed successfully.
 */
static bool parseJsonInt32(const char *json, const char *key, int32_t *outValue) {
    int intValue;
    if (!parseJsonInt(json, key, &intValue)) {
        return false;
    }
    *outValue = intValue;
    return true;
}

MqttGatewayClient::MqttGatewayClient(ConnectivityDriver &transportDriver, Client &networkClient,
                                     const char *brokerIpAddress, const uint16_t brokerPort,
                                     const char *publishT, const char *subscribeT,
                                     Device *targetDev,
                                     const char *deviceId)
    : GatewayClient(transportDriver, ""), // We don't use serviceEndpointUrl here, so pass ""
      mqttClient(networkClient),
      clientIdentifier(deviceId),
      publishTopic(publishT),
      subscribeTopic(subscribeT),
      lastReconnectAttemptTimestamp(0),
      reconnectIntervalInMilliseconds(DEFAULT_RECONNECT_INTERVAL_MILLISECONDS),
      sendsSucceededCount(0),
      sendsFailedCount(0),
      firstFailureLogged(false),
      serializationBuffer{},
      targetDevice(targetDev),
      subscribePayloadBuffer{} {
    mqttClient.begin(brokerIpAddress, brokerPort, networkClient);

    // Register the static callback if subscribe is enabled
    if (subscribeTopic != nullptr) {
        gActiveMqttClient = this;
        mqttClient.onMessage(mqttMessageCallback);
    }
}

MqttGatewayClient::MqttGatewayClient(ConnectivityDriver &transportDriver, Client &networkClient,
                                     const char *brokerIpAddress, const uint16_t brokerPort,
                                     const char *topic, const char *deviceId)
    : GatewayClient(transportDriver, ""), // We don't use serviceEndpointUrl here, so pass ""
      mqttClient(networkClient),
      clientIdentifier(deviceId),
      publishTopic(topic),
      subscribeTopic(nullptr),
      lastReconnectAttemptTimestamp(0),
      reconnectIntervalInMilliseconds(DEFAULT_RECONNECT_INTERVAL_MILLISECONDS),
      sendsSucceededCount(0),
      sendsFailedCount(0),
      firstFailureLogged(false),
      serializationBuffer{},
      targetDevice(nullptr),
      subscribePayloadBuffer{} {
    mqttClient.begin(brokerIpAddress, brokerPort, networkClient);
}

void MqttGatewayClient::ensureBrokerSessionConnection() {
    // If the network interface state is broken, do not attempt a broker connection
    if (!communicationTransportDriver.canTransmit()) {
        return;
    }

    if (mqttClient.connected()) {
        return;
    }

    // Rate-limit connection attempts to avoid blocking the network task during outages
    if (const unsigned long currentTimestamp = millis(); lastReconnectAttemptTimestamp == 0 ||
                                                         currentTimestamp - lastReconnectAttemptTimestamp >=
                                                         reconnectIntervalInMilliseconds) {
        lastReconnectAttemptTimestamp = currentTimestamp;
        MODEST_LOG_I("MQTT", "Attempting connection to broker for client ID: %s", clientIdentifier);

        if (mqttClient.connect(clientIdentifier)) {
            MODEST_LOG_I("MQTT", "Connected to MQTT broker successfully.");
            // Subscribe to the inbound command topic after successful connection
            subscribeToTopic();
        } else {
            MODEST_LOG_W("MQTT", "Failed to connect to MQTT broker, error: %d", mqttClient.lastError());
        }
    }
}

void MqttGatewayClient::service() {
    if (!communicationTransportDriver.canTransmit()) {
        communicationTransportDriver.connect();
        return;
    }

    ensureBrokerSessionConnection();
    if (mqttClient.connected()) {
        mqttClient.loop();
    }
}

void MqttGatewayClient::loop() {
    service();
}

bool MqttGatewayClient::isConnected() {
    return communicationTransportDriver.isConnected() && mqttClient.connected();
}

int MqttGatewayClient::getClientState() {
    return mqttClient.lastError();
}

void MqttGatewayClient::setReconnectInterval(const unsigned long intervalInMilliseconds) {
    reconnectIntervalInMilliseconds = intervalInMilliseconds;
}

unsigned long MqttGatewayClient::getReconnectInterval() const {
    return reconnectIntervalInMilliseconds;
}

bool MqttGatewayClient::sendTelemetryRecord(const TelemetryPackage &telemetryPackage) {
    // Check transport capability before processing
    if (!communicationTransportDriver.canTransmit()) {
        communicationTransportDriver.connect(); // Asynchronously attempt a link recovery
        sendsFailedCount++;
        if (!firstFailureLogged) {
            MODEST_LOG_W("MQTT", "First send failure: transport not ready");
            firstFailureLogged = true;
        }
        return false;
    }

    // Ensure MQTT broker session is established
    ensureBrokerSessionConnection();
    if (!mqttClient.connected()) {
        sendsFailedCount++;
        if (!firstFailureLogged) {
            MODEST_LOG_W("MQTT", "First send failure: broker not connected");
            firstFailureLogged = true;
        }
        return false;
    }

    // Serialize telemetry payload into bounded buffer
    if (const size_t serializedLength = telemetryPackage.serializeToBuffer(
        serializationBuffer, MAX_TELEMETRY_PAYLOAD_SIZE); serializedLength == 0) {
        MODEST_LOG_W("MQTT", "Payload too large for serialization buffer (%u bytes)", MAX_TELEMETRY_PAYLOAD_SIZE);
        sendsFailedCount++;
        if (!firstFailureLogged) {
            MODEST_LOG_W("MQTT", "First send failure: payload exceeds buffer size");
            firstFailureLogged = true;
        }
        return false;
    }

    // Publish telemetry payload to the broker topic
    const bool publishSuccessStatus = mqttClient.
            publish(publishTopic, serializationBuffer, strlen(serializationBuffer));
    if (!publishSuccessStatus) {
        sendsFailedCount++;
        if (!firstFailureLogged) {
            MODEST_LOG_W("MQTT", "First send failure: publish to topic %s failed", publishTopic);
            firstFailureLogged = true;
        }
    } else {
        sendsSucceededCount++;
        MODEST_LOG_D("MQTT", "Published telemetry payload to topic: %s", publishTopic);
    }

    // Maintain internal background MQTT processing states (pings, heartbeats)
    mqttClient.loop();

    return publishSuccessStatus;
}

GatewayStatistics MqttGatewayClient::getStatistics() const {
    GatewayStatistics stats;
    stats.sendsSucceeded = sendsSucceededCount;
    stats.sendsFailed = sendsFailedCount;
    return stats;
}

const char *MqttGatewayClient::getSubscribeTopic() const {
    return subscribeTopic;
}

/**
 * @name Cloud-to-device MQTT subscribe
 * @{
 *
 * @brief Subscribes to an MQTT topic for inbound commands and marshals
 * parsed JSON payloads to the Device via enqueueCommand().
 */

void MqttGatewayClient::subscribeToTopic() {
    if (subscribeTopic == nullptr) {
        return;
    }

    if (mqttClient.subscribe(subscribeTopic, DEFAULT_MQTT_QOS)) {
        MODEST_LOG_I("MQTT", "Subscribed to topic: %s", subscribeTopic);
    } else {
        MODEST_LOG_W("MQTT", "Failed to subscribe to topic: %s", subscribeTopic);
    }
}

void MqttGatewayClient::mqttMessageCallback(String &topic, String &payload) {
    (void) topic; // Used only in MODEST_LOG_D, which may be compiled out
    if (gActiveMqttClient == nullptr || gActiveMqttClient->targetDevice == nullptr) {
        return;
    }

    // Guard: ignore messages that don't match the subscribe topic
    if (gActiveMqttClient->subscribeTopic == nullptr) {
        return;
    }

    // Safely copy payload into fixed buffer
    const size_t length = payload.length();
    if (length >= static_cast<size_t>(MAX_SUBSCRIBE_PAYLOAD_SIZE)) {
        MODEST_LOG_W("MQTT", "Incoming command payload too large (%d bytes)", length);
        return;
    }

    // Null-terminate the payload for JSON parsing
    memcpy(gActiveMqttClient->subscribePayloadBuffer, payload.c_str(), length);
    gActiveMqttClient->subscribePayloadBuffer[length] = '\0';

    // Parse JSON to extract command identifier using native deserializer
    int commandId;
    if (!parseJsonInt(gActiveMqttClient->subscribePayloadBuffer, JSON_KEY_COMMAND_IDENTIFIER, &commandId)) {
        MODEST_LOG_W("MQTT", "Incoming command missing command identifier field");
        return;
    }

    int32_t commandPayload = 0;
    parseJsonInt32(gActiveMqttClient->subscribePayloadBuffer, JSON_KEY_COMMAND_PAYLOAD, &commandPayload);
    MODEST_LOG_D("MQTT", "Received command %d (payload %ld) on topic: %s", commandId, commandPayload, topic.c_str());

    // Marshal the command to the sampler task via Device::enqueueCommand()
    // The Device is both the target handler and the marshaler, ensuring cross-task safety
    const Command command(commandId, commandPayload);
    gActiveMqttClient->targetDevice->enqueueCommand(gActiveMqttClient->targetDevice, command);
}

/** @} */
