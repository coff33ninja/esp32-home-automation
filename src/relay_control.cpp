/* Relay control implementation moved from include/relay_control.h */
#include "../include/relay_control.h"
#include <Arduino.h>

// Global variable definitions (single definition)
RelayStates relayStates = {false, false, false, false};
bool relayControlInitialized = false;

void initRelayControl() {
    // Set all relays to OFF (fail-safe)
    relayStates.relay1 = false;
    relayStates.relay2 = false;
    relayStates.relay3 = false;
    relayStates.relay4 = false;
    
    digitalWrite(RELAY_1_PIN, LOW);
    digitalWrite(RELAY_2_PIN, LOW);
    digitalWrite(RELAY_3_PIN, LOW);
    digitalWrite(RELAY_4_PIN, LOW);
    
    relayControlInitialized = true;
    DEBUG_PRINTLN("[RELAY] Relay control initialized - all OFF");
}

void setRelay(RelayChannel channel, bool state) {
    int pin;
    bool* statePtr;
    const char* channelName;
    
    // Map channel to pin and state variable
    switch (channel) {
        case RELAY_1:
            pin = RELAY_1_PIN;
            statePtr = &relayStates.relay1;
            channelName = "Relay 1";
            break;
        case RELAY_2:
            pin = RELAY_2_PIN;
            statePtr = &relayStates.relay2;
            channelName = "Relay 2";
            break;
        case RELAY_3:
            pin = RELAY_3_PIN;
            statePtr = &relayStates.relay3;
            channelName = "Relay 3";
            break;
        case RELAY_4:
            pin = RELAY_4_PIN;
            statePtr = &relayStates.relay4;
            channelName = "Relay 4";
            break;
        default:
            DEBUG_PRINTLN("[RELAY] Invalid channel");
            return;
    }
    
    // Update state
    *statePtr = state;
    digitalWrite(pin, state ? HIGH : LOW);
    
    DEBUG_PRINTF("[RELAY] %s set to %s\n", channelName, state ? "ON" : "OFF");
}

void toggleRelay(RelayChannel channel) {
    bool currentState = getRelayState(channel);
    setRelay(channel, !currentState);
}

bool getRelayState(RelayChannel channel) {
    switch (channel) {
        case RELAY_1: return relayStates.relay1;
        case RELAY_2: return relayStates.relay2;
        case RELAY_3: return relayStates.relay3;
        case RELAY_4: return relayStates.relay4;
        default: return false;
    }
}

void setAllRelays(bool state) {
    setRelay(RELAY_1, state);
    setRelay(RELAY_2, state);
    setRelay(RELAY_3, state);
    setRelay(RELAY_4, state);
    
    DEBUG_PRINTF("[RELAY] All relays set to %s\n", state ? "ON" : "OFF");
}

void publishRelayStates() {
    // This function can be called from main.cpp to publish states via MQTT
    DEBUG_PRINTF("[RELAY] States - R1:%d R2:%d R3:%d R4:%d\n",
                 relayStates.relay1, relayStates.relay2, 
                 relayStates.relay3, relayStates.relay4);
}
