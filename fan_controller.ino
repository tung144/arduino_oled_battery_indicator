#include <Servo.h>

const int pinMinus = 2;
const int pinPlus  = 3;

// LEDs for bar A,B,C,D,F
const int ledPins[5] = {4,5,6,7,8};
const int ledG = 10;   // battery/charging indicator
const int ledH = 11;   // battery/charging indicator

const int escPin = 9;  // ESC control pin
const int battPin = A0; // battery voltage divider input
const int chargeDetectPin = A1; // charger detection input

Servo esc;

int speedLevel = 0; // 0..10
bool fanOn = false;
unsigned long lastDisplaySwitch = 0;

enum DisplayMode {DISPLAY_SPEED, DISPLAY_BATTERY};
DisplayMode displayMode = DISPLAY_SPEED;

int readBatteryPercent() {
  int raw = analogRead(battPin);
  // Voltage divider: R1=6.8k (top), R2=10k (bottom)
  float voltage = (raw / 1023.0) * 5.0 * (6.8 + 10.0) / 10.0;
  float percent = (voltage - 3.0) / (4.2 - 3.0); // 0% at 3.0V, 100% at 4.2V
  percent = constrain(percent, 0.0, 1.0);
  return (int)(percent * 100.0);
}

bool isCharging() {
  // if charger LED (A1) has voltage, assume charger connected
  return analogRead(chargeDetectPin) > 100; // threshold ~0.5V
}

void setEscSpeed(int level) {
  // map 0..10 levels to 1000..2000 microseconds (adjust as needed)
  int pwm = map(level, 0, 10, 1000, 2000);
  esc.writeMicroseconds(pwm);
}

void showBarLevel(int level) {
  // level 0..10 displayed with 5 LEDs, blinking for half levels
  for (int i=0;i<5;i++) digitalWrite(ledPins[i], LOW);
  int full = level / 2;
  bool half = level % 2;
  for (int i=0;i<full && i<5;i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  if (half && full < 5) {
    digitalWrite(ledPins[full], (millis()/300)%2 ? HIGH : LOW);
  }
}

void showSpeed(int level) {
  showBarLevel(level);
  digitalWrite(ledG, LOW);
  digitalWrite(ledH, LOW);
}

void showBattery(int percent) {
  int level = map(percent, 0, 100, 0, 10);
  showBarLevel(level);
  if (percent >= 95) {
    digitalWrite(ledH, HIGH);
    digitalWrite(ledG, LOW);
  } else {
    digitalWrite(ledG, HIGH);
    digitalWrite(ledH, LOW);
  }
}

void setup() {
  for (int i=0;i<5;i++) pinMode(ledPins[i], OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledH, OUTPUT);
  pinMode(pinPlus, INPUT_PULLUP);
  pinMode(pinMinus, INPUT_PULLUP);
  esc.attach(escPin);
}

void loop() {
  int battery = readBatteryPercent();
  bool charging = isCharging();

  if (!fanOn) {
    if (charging) {
      displayMode = DISPLAY_BATTERY;
      showBattery(battery);
    } else {
      showSpeed(0);
    }
    if (digitalRead(pinMinus) == LOW) {
      unsigned long start = millis();
      while (digitalRead(pinMinus) == LOW) {
        if (millis() - start > 1000) {
          fanOn = true;
          speedLevel = 1;
          setEscSpeed(speedLevel);
          break;
        }
      }
    }
  } else {
    if (digitalRead(pinPlus) == LOW) {
      if (speedLevel < 10) {
        speedLevel++;
        setEscSpeed(speedLevel);
        displayMode = DISPLAY_SPEED;
      } else {
        displayMode = DISPLAY_BATTERY;
      }
      lastDisplaySwitch = millis();
      while (digitalRead(pinPlus) == LOW);
    }
    if (digitalRead(pinMinus) == LOW) {
      if (speedLevel > 1) {
        speedLevel--;
        setEscSpeed(speedLevel);
        displayMode = DISPLAY_SPEED;
      } else {
        displayMode = DISPLAY_BATTERY;
      }
      lastDisplaySwitch = millis();
      while (digitalRead(pinMinus) == LOW);
    }

    // Low battery behavior
    if (battery < 15) {
      if (speedLevel > 5) {
        speedLevel = 5;
        setEscSpeed(speedLevel);
      }
      digitalWrite(ledG, (millis()/500)%2);
    } else if (battery < 30) {
      digitalWrite(ledH, (millis()/500)%2);
    }

    if (charging) {
      displayMode = DISPLAY_BATTERY;
      lastDisplaySwitch = millis();
    } else if (millis() - lastDisplaySwitch > 1000) {
      displayMode = DISPLAY_SPEED;
    }

    if (displayMode == DISPLAY_SPEED) {
      showSpeed(speedLevel);
    } else {
      showBattery(battery);
    }
  }
}

