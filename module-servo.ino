#define SERVO_PIN 17
#define COM_PIN 22

bool memory = 0;

void setServoAngle(int angle) {
  if (angle < 0)   angle = 0;
  if (angle > 180) angle = 180;

  int pulse_us = 500 + (angle * (2400 - 500)) / 180;
  uint32_t duty = ((uint64_t)pulse_us * 65535) / 20000;

  ledcWrite(SERVO_PIN, duty);
}

void setup() {
  ledcAttach(SERVO_PIN, 50, 16);
  pinMode(COM_PIN, INPUT_PULLUP);
  setServoAngle(0);
}

void loop() {
  bool pin_state = digitalRead(COM_PIN);
  delay(20);
  if (pin_state == digitalRead(COM_PIN)){
    if (pin_state != memory){
      if (pin_state == 1){
        setServoAngle(90);
      } else {
        setServoAngle(0);
      }
      memory = pin_state;
    }
  }
}