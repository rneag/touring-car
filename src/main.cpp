#include <Arduino.h>

const int IN1 = 5;
const int IN2 = 6;
const int IN3 = 9;
const int IN4 = 10;

const int TRIG_PIN = 2;
const int ECHO_PIN = 3;

const int CRASH_THRESHOLD = 15;
const int SAFE_DISTANCE = 30;

const int MAX_SPEED = 255;
const int MIN_SPEED = 130;
const int ACCEL_STEP = 15;
const int ACCEL_DELAY_MS = 20;

char currentCommand = 'S';
int currentSpeed = 0;
unsigned long lastAccelTime = 0;
volatile bool btDataReady = false;
volatile char btBuffer = 0;

void serialEvent() {
    while (Serial.available()) {
        char received = Serial.read();
        if (received != '\n' && received != '\r' && received != ' ') {
            if (strchr("FBLRS", received) != NULL) {
                btBuffer = received;
                btDataReady = true;
            }
        }
    }
}

void stopCar() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    currentSpeed = 0;
}

void setForwardSpeed(int speed) {
    analogWrite(IN1, speed);
    digitalWrite(IN2, LOW);
    analogWrite(IN3, speed);
    digitalWrite(IN4, LOW);
}

void setBackwardSpeed(int speed) {
    digitalWrite(IN1, LOW);
    analogWrite(IN2, speed);
    digitalWrite(IN3, LOW);
    analogWrite(IN4, speed);
}

void turnLeft() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
}

void turnRight() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
}

long readDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) return 999;
    return duration / 58;
}

long readDistanceStable() {
    int validCount = 0;
    long sum = 0;

    for (int i = 0; i < 3; i++) {
        long d = readDistance();
        if (d != 999) {
            sum += d;
            validCount++;
        }
        delay(10);
    }

    if (validCount == 0) return 999;
    return sum / validCount;
}

void scanAndAlign() {
    turnLeft();
    delay(300);
    stopCar();
    delay(200);
    long distLeft = readDistanceStable();

    turnRight();
    delay(600);
    stopCar();
    delay(200);
    long distRight = readDistanceStable();

    if (distLeft > distRight) {
        turnLeft();
        delay(600);
        stopCar();
    } else {
        stopCar();
    }
}

void autonomousEvasion() {
    stopCar();
    delay(200);

    int reverseTimeout = 0;

    while (readDistanceStable() < SAFE_DISTANCE && reverseTimeout < 50) {
        setBackwardSpeed(MAX_SPEED);
        delay(50);
        reverseTimeout++;
    }

    stopCar();
    delay(300);

    scanAndAlign();

    btDataReady = false;
    btBuffer = 0;

    currentCommand = 'S';
}

void handleAcceleration() {
    unsigned long now = millis();
    if (now - lastAccelTime < ACCEL_DELAY_MS) return;
    lastAccelTime = now;

    if (currentSpeed == 0) {
        currentSpeed = MIN_SPEED;
    } else if (currentSpeed < MAX_SPEED) {
        currentSpeed = min(currentSpeed + ACCEL_STEP, MAX_SPEED);
    }
}

void setup() {
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    stopCar();

    Serial.begin(9600);
}

void loop() {
    if (btDataReady) {
        char received = btBuffer;
        btDataReady = false;

        if (received != currentCommand) {
            currentSpeed = 0;
        }
        currentCommand = received;
    }

    long distance = readDistanceStable();

    if (currentCommand == 'F' && distance <= CRASH_THRESHOLD) {
        autonomousEvasion();
        return;
    }

    switch (currentCommand) {
        case 'F':
            handleAcceleration();
            setForwardSpeed(currentSpeed);
            break;
        case 'B':
            handleAcceleration();
            setBackwardSpeed(currentSpeed);
            break;
        case 'L': turnLeft(); break;
        case 'R': turnRight(); break;
        case 'S': stopCar(); break;
    }
}
