#include "../include/motor_control.h"
#include <Arduino.h>
#include "config.h"

// Single-definition globals
MotorState currentMotorState = MOTOR_STOPPED;
int currentMotorSpeed = 0;
bool motorControlInitialized = false;

void initMotorControl() {
    // Configure PWM for motor control
    ledcSetup(MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(MOTOR_PWM_PIN, MOTOR_PWM_CHANNEL);
    
    // Ensure motor is stopped
    stopMotor();
    
    motorControlInitialized = true;
    DEBUG_PRINTLN("[MOTOR] Motor control initialized");
}

void stopMotor() {
    digitalWrite(MOTOR_PIN_A, LOW);
    digitalWrite(MOTOR_PIN_B, LOW);
    ledcWrite(MOTOR_PWM_CHANNEL, 0);
    currentMotorState = MOTOR_STOPPED;
    currentMotorSpeed = 0;
    
    DEBUG_PRINTLN("[MOTOR] Motor stopped");
}

void setMotorDirection(MotorState direction) {
    currentMotorState = direction;
    
    switch (direction) {
        case MOTOR_FORWARD:
            digitalWrite(MOTOR_PIN_A, HIGH);
            digitalWrite(MOTOR_PIN_B, LOW);
            DEBUG_PRINTLN("[MOTOR] Direction: FORWARD");
            break;
        
        case MOTOR_REVERSE:
            digitalWrite(MOTOR_PIN_A, LOW);
            digitalWrite(MOTOR_PIN_B, HIGH);
            DEBUG_PRINTLN("[MOTOR] Direction: REVERSE");
            break;
        
        case MOTOR_STOPPED:
        default:
            stopMotor();
            break;
    }
}

void setMotorSpeed(int speed) {
    // Clamp speed to valid range (0-255)
    speed = constrain(speed, 0, 255);
    currentMotorSpeed = speed;
    
    ledcWrite(MOTOR_PWM_CHANNEL, speed);
    
    DEBUG_PRINTF("[MOTOR] Speed set to: %d\n", speed);
}

int readPotPosition() {
    // Read ADC value with smoothing
    const int numSamples = 5;
    long sum = 0;
    
    for (int i = 0; i < numSamples; i++) {
        sum += analogRead(POT_ADC_PIN);
        delay(2);
    }
    
    return sum / numSamples;
}

void moveMotorToPosition(int targetPosition) {
    int currentPosition = readPotPosition();
    int error = targetPosition - currentPosition;
    
    // Deadband - don't move if close enough
    if (abs(error) < POT_DEADBAND) {
        stopMotor();
        return;
    }
    
    // Determine direction
    if (error > 0) {
        setMotorDirection(MOTOR_FORWARD);
    } else {
        setMotorDirection(MOTOR_REVERSE);
    }
    
    // Simple proportional speed control
    int speed = constrain(abs(error) / 10, 50, 200);
    setMotorSpeed(speed);
    
    DEBUG_PRINTF("[MOTOR] Moving to position: %d (current: %d, error: %d)\n", 
                 targetPosition, currentPosition, error);
}
