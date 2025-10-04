// Compatibility wrappers for functions used with different signatures across modules
#include "relay_control.h"
#include "led_effects.h"
#include "ir_handler.h"

// Provide a single-definition global for volumeMuted
bool volumeMuted = false;

// Wrapper expecting integer channel (used by some modules/IR handler)
void toggleRelay(int channel) {
    // Convert to enum and call the typed API
    toggleRelay((RelayChannel)channel);
}

// Wrapper expecting integer effect id
void setEffect(int effect) {
    setEffect((LEDEffect)effect);
}
