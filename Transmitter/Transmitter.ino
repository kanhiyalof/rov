#include <WiFi.h>
#include <esp_now.h>

// ==================================================
// JOYSTICK
//
// Wired to explicit GPIO numbers instead of board
// aliases like "D1"/"D2" (a bare ESP32 DevKit v1 has
// no such silkscreen labels anyway - those pins are
// just GPIO numbers).
//
// JOY_X and JOY_Y must both be ADC1 pins (32-39).
// ADC2 pins (0,2,4,12-15,25-27) are NOT usable for
// analogRead() while WiFi is active, which ESP-NOW
// requires - they silently return garbage/0 whenever
// the radio is on.
// ==================================================

#define JOY_X   36   // ADC1_CH0 - same physical pin the old "A0" alias used
#define JOY_Y   34   // ADC1_CH6 - previously "D1", which was NOT a working ADC pin
#define JOY_SW  27   // plain digital input, ADC channel doesn't matter here

// ==================================================
// RX MAC ADDRESS
// ==================================================

uint8_t receiverAddress[] = {
  0xC8, 0xC9, 0xA3, 0x69, 0x73, 0xA5
};

// ==================================================
// CALIBRATION
//
// analogReadResolution(10) below keeps readings in the
// same 0-1023 scale the ESP8266 receiver's mapping code
// expects (ESP32's ADC defaults to 12-bit / 0-4095).
//
// X_CENTER carries over from the old board's X pot.
// Y_CENTER is a generic mid-scale placeholder for the
// newly-wired Y pin - re-measure it with the serial
// monitor (read rawY at rest) and correct it before
// trusting reverse/forward behavior.
// ==================================================

#define X_CENTER 829
#define Y_CENTER 512

#define DEAD_ZONE 100

// ==================================================
// DATA
// ==================================================

struct ControlData {

  int x;
  int y;
  bool button;
};

ControlData data;

// ==================================================
// SEND CALLBACK
// ==================================================

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {

  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  analogReadResolution(10);

  pinMode(JOY_SW, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 RC CAR - TRANSMITTER");
  Serial.println("================================");

  Serial.print("TX MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP-NOW initialization failed!");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {

    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("ESP-NOW READY");
  Serial.println("Joystick READY");
}

// ==================================================
// LOOP
// ==================================================

void loop() {

  // ------------------------------------------------
  // READ RAW VALUES
  // ------------------------------------------------

  int rawX = analogRead(JOY_X);
  int rawY = analogRead(JOY_Y);

  // ------------------------------------------------
  // BUTTON
  // ------------------------------------------------

  data.button = !digitalRead(JOY_SW);

  // ------------------------------------------------
  // CHECK IDLE
  // ------------------------------------------------

  bool idleX = abs(rawX - X_CENTER) <= DEAD_ZONE;
  bool idleY = abs(rawY - Y_CENTER) <= DEAD_ZONE;

  bool joystickIdle = idleX && idleY;

  // ------------------------------------------------
  // IF IDLE
  // SEND EXACT CENTER VALUES
  // ------------------------------------------------

  if (joystickIdle) {

    data.x = X_CENTER;
    data.y = Y_CENTER;

  }
  else {

    data.x = rawX;
    data.y = rawY;
  }

  // ------------------------------------------------
  // SERIAL MONITOR
  // ------------------------------------------------

  Serial.print("RAW X: ");
  Serial.print(rawX);

  Serial.print(" | RAW Y: ");
  Serial.print(rawY);

  Serial.print(" | ");

  if (joystickIdle) {

    Serial.print("IDLE -> STOP");
  }
  else {

    Serial.print("MOVING");
  }

  Serial.print(" | Button: ");
  Serial.println(data.button);

  // ------------------------------------------------
  // SEND
  // ------------------------------------------------

  esp_now_send(
    receiverAddress,
    (uint8_t *)&data,
    sizeof(data)
  );

  delay(50);
}
