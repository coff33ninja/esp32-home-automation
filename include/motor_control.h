/**
 * Motor Control Module
 * 
 * Controls the motorized potentiometer using L298N H-Bridge driver.
 * Implements smooth motor control with direction and speed management.
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "config.h"

// Motor states
enum MotorState {
    MOTOR_STOPPED,
    MOTOR_FORWARD,
    MOTOR_REVERSE
};

// Global variables
extern MotorState currentMotorState;
extern int currentMotorSpeed;
extern bool motorControlInitialized;

// Function prototypes
void initMotorControl();
void setMotorSpeed(int speed);
void setMotorDirection(MotorState direction);
void stopMotor();
void moveMotorToPosition(int targetPosition);
int readPotPosition();
#endif // MOTOR_CONTROL_H
