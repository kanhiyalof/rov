#include <ESP8266WiFi.h>
#include <espnow.h>

// ==================================================
// L298N MOTOR PINS
// ==================================================

#define ENA D7

#define IN1 D5

#define IN2 D6

#define ENB D1

#define IN3 D3

#define IN4 D2




// ==================================================
// MAXIMUM SPEED
// 460 / 1023 ≈ 45%
// ==================================================

#define MAX_SPEED 460

// ==================================================
// JOYSTICK X CALIBRATION
// ==================================================

#define X_LEFT   0
#define X_CENTER 770
#define X_RIGHT  1023

// ==================================================
// JOYSTICK Y CALIBRATION
//
// Measured directly: pushing backward drove the raw
// reading to 0, pushing forward drove it to 1023 - high
// means forward on this wiring, opposite of the original
// placeholder assumption.
// ==================================================

#define Y_BACKWARD 0
#define Y_CENTER   750
#define Y_FORWARD  1023

// ==================================================
// DEAD ZONE
// ==================================================

#define DEAD_ZONE 60

// ==================================================
// FAILSAFE
// ==================================================

#define FAILSAFE_TIME 500

unsigned long lastReceiveTime = 0;

// ==================================================
// DATA STRUCTURE
// ==================================================

struct ControlData {

  int x;
  int y;
  bool button;
};

ControlData data;

// ==================================================
// STOP MOTORS
// ==================================================

void stopMotors() {

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ==================================================
// MOTOR A
// ==================================================

void setMotorA(int speedValue) {

  speedValue = constrain(speedValue, -MAX_SPEED, MAX_SPEED);

  if (speedValue > 0) {

    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);

    analogWrite(ENA, speedValue);
  }

  else if (speedValue < 0) {

    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);

    analogWrite(ENA, -speedValue);
  }

  else {

    analogWrite(ENA, 0);

    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }
}

// ==================================================
// MOTOR B
// ==================================================

void setMotorB(int speedValue) {

  speedValue = constrain(speedValue, -MAX_SPEED, MAX_SPEED);

  if (speedValue > 0) {

    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);

    analogWrite(ENB, speedValue);
  }

  else if (speedValue < 0) {

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);

    analogWrite(ENB, -speedValue);
  }

  else {

    analogWrite(ENB, 0);

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }
}

// ==================================================
// ESP-NOW RECEIVE
// ==================================================

void onDataReceive(
  uint8_t *mac,
  uint8_t *incomingData,
  uint8_t len
) {

  if (len == sizeof(ControlData)) {

    memcpy(&data, incomingData, sizeof(data));

    lastReceiveTime = millis();
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Always start stopped
  stopMotors();

  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.println("==================================");
  Serial.println("       RC CAR RECEIVER");
  Serial.println("==================================");

  Serial.print("RX MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != 0) {

    Serial.println("ESP-NOW FAILED");

    stopMotors();

    while (true) {

      stopMotors();
      delay(100);
    }
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  esp_now_register_recv_cb(onDataReceive);

  Serial.println("ESP-NOW READY");
  Serial.println("MOTORS STOPPED");

  lastReceiveTime = millis();
}

// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  // ==================================================
  // COMMUNICATION FAILSAFE
  // ==================================================

  if (millis() - lastReceiveTime > FAILSAFE_TIME) {

    stopMotors();
    return;
  }

  // ==================================================
  // GET JOYSTICK VALUES
  // ==================================================

  int x = data.x;
  int y = data.y;

  // ==================================================
  // STEERING CALCULATION
  // ==================================================

  int steering = 0;

  // LEFT SIDE
  if (x < X_CENTER - DEAD_ZONE) {

    steering = map(
      x,
      X_LEFT,
      X_CENTER - DEAD_ZONE,
      -MAX_SPEED,
      0
    );
  }

  // RIGHT SIDE
  else if (x > X_CENTER + DEAD_ZONE) {

    steering = map(
      x,
      X_CENTER + DEAD_ZONE,
      X_RIGHT,
      0,
      MAX_SPEED
    );
  }

  // CENTER
  else {

    steering = 0;
  }

  steering = constrain(
    steering,
    -MAX_SPEED,
    MAX_SPEED
  );

  // ==================================================
  // Y AXIS (FORWARD / REVERSE)
  // ==================================================

  int throttle = 0;

  // FORWARD
  if (y > Y_CENTER + DEAD_ZONE) {

    throttle = map(
      y,
      Y_CENTER + DEAD_ZONE,
      Y_FORWARD,
      0,
      MAX_SPEED
    );
  }

  // REVERSE
  else if (y < Y_CENTER - DEAD_ZONE) {

    throttle = map(
      y,
      Y_BACKWARD,
      Y_CENTER - DEAD_ZONE,
      -MAX_SPEED,
      0
    );
  }

  // CENTER
  else {

    throttle = 0;
  }

  throttle = constrain(
    throttle,
    -MAX_SPEED,
    MAX_SPEED
  );

  // ==================================================
  // IDLE
  // ==================================================

  if (steering == 0 && throttle == 0) {

    stopMotors();

    Serial.print("X: ");
    Serial.print(x);

    Serial.print(" | Y: ");
    Serial.print(y);

    Serial.println(" | STOP");

    delay(10);

    return;
  }

  // ==================================================
  // DIFFERENTIAL STEERING
  // ==================================================

  int leftMotor;
  int rightMotor;

  leftMotor = throttle + steering;
  rightMotor = throttle - steering;

  // ==================================================
  // LIMIT MOTOR SPEED
  // ==================================================

  leftMotor = constrain(
    leftMotor,
    -MAX_SPEED,
    MAX_SPEED
  );

  rightMotor = constrain(
    rightMotor,
    -MAX_SPEED,
    MAX_SPEED
  );

  // ==================================================
  // OUTPUT
  // ==================================================

  setMotorA(leftMotor);
  setMotorB(rightMotor);

  // ==================================================
  // DEBUG
  // ==================================================

  Serial.print("X: ");
  Serial.print(x);

  Serial.print(" | Y: ");
  Serial.print(y);

  Serial.print(" | Steering: ");
  Serial.print(steering);

  Serial.print(" | Throttle: ");
  Serial.print(throttle);

  Serial.print(" | LEFT: ");
  Serial.print(leftMotor);

  Serial.print(" | RIGHT: ");
  Serial.println(rightMotor);

  delay(10);
}
