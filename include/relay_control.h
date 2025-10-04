/**
 * Relay Control Module
 * 
 * Controls 4-channel relay module for lighting control.
 * Supports on/off switching and state management.
 */

#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>
#include "config.h"

// Relay channels
enum RelayChannel {
    RELAY_1,
    RELAY_2,
    RELAY_3,
    RELAY_4
};

// Relay states
struct RelayStates {
    bool relay1;
    bool relay2;
    bool relay3;
    bool relay4;
};

// Global variables
extern RelayStates relayStates;
extern bool relayControlInitialized;

// Function prototypes
void initRelayControl();
void setRelay(RelayChannel channel, bool state);
void toggleRelay(RelayChannel channel);
bool getRelayState(RelayChannel channel);
void setAllRelays(bool state);
void publishRelayStates();

// Note: Implementations and globals moved to src/relay_control.cpp

#endif // RELAY_CONTROL_H
