/*
  Automatic dog feeder (Arduino)
  - An RTC (DS1307) triggers each dispense.
  - A servo turns a 4-compartment separator by one step per dispense.
  - A buzzer signals the dispense; an I2C LCD shows the clock and the status.

  Wiring: servo signal -> D8, buzzer -> D5, LCD (0x27) and RTC on I2C (SDA/SCL), 5 V and GND.

  Calibrate HOME_ANGLE and STEP_ANGLE on the real separator (see README).
*/
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo;
RTC_DS1307 rtc;

const int servoPin  = 8;
const int buzzerPin = 5;

// --- Separator / servo settings (calibrate on the real device) ---
const int HOME_ANGLE       = 0;    // separator position before the first dispense
const int STEP_ANGLE       = 45;   // one compartment = 45 degrees
const int NB_DISPENSES     = 4;    // compartments per cycle
const int MOVEMENT_DELAY   = 50;   // ms per degree (slow, smooth movement)

// The servo can only reach 0..180 degrees: refuse to compile a layout that cannot fit.
static_assert(HOME_ANGLE >= 0 && HOME_ANGLE + NB_DISPENSES * STEP_ANGLE <= 180,
              "HOME_ANGLE + NB_DISPENSES * STEP_ANGLE must stay within 0..180 degrees");

// --- Timing ---
const int TRIGGER_INTERVAL_S            = 10;    // demo mode: one trigger every 10 s (use a larger value for real use)
const unsigned long BUZZER_DURATION_MS  = 1000;
const unsigned long MESSAGE_DURATION_MS = 2000;

const char *const STEP_NAMES[NB_DISPENSES] = {"uno", "dos", "tres", "quatro"};

int angle = HOME_ANGLE;
int dispenseCount = 0;
int lastTriggerSecond = -1;

bool buzzerActive = false;
unsigned long buzzerStartTime = 0;

bool showDispenseMessage = false;
unsigned long dispenseMessageStart = 0;

// Move the servo slowly to targetAngle (one degree at a time).
void moveServoTo(int targetAngle) {
  while (angle < targetAngle) { angle++; servo.write(angle); delay(MOVEMENT_DELAY); }
  while (angle > targetAngle) { angle--; servo.write(angle); delay(MOVEMENT_DELAY); }
}

void print2(int v) {            // print a number on 2 digits
  if (v < 10) lcd.print("0");
  lcd.print(v);
}

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  if (!rtc.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("RTC not found!");
    while (1);
  }
  if (!rtc.isrunning()) {       // RTC lost power: set it to the compile time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.setCursor(0, 0);
  lcd.print("Bonjour!!!");
  delay(2000);

  // Attach first, then send the home position (write() before attach() has no effect).
  servo.attach(servoPin);
  servo.write(HOME_ANGLE);
  angle = HOME_ANGLE;
  delay(500);                   // let the servo settle
}

void loop() {
  DateTime now = rtc.now();
  int second = now.second();

  // Show the clock unless a dispense message is displayed
  if (!showDispenseMessage) {
    lcd.setCursor(0, 0);
    lcd.print(now.year()); lcd.print("/");
    print2(now.month()); lcd.print("/");
    print2(now.day());
    lcd.print("     ");

    lcd.setCursor(0, 1);
    print2(now.hour()); lcd.print(":");
    print2(now.minute()); lcd.print(":");
    print2(second);
    lcd.print("  ");
  }

  // Trigger every TRIGGER_INTERVAL_S seconds (once per matching second)
  if (second % TRIGGER_INTERVAL_S == 0 && second != lastTriggerSecond) {
    lastTriggerSecond = second;

    if (dispenseCount < NB_DISPENSES) {
      // Turn the separator by one compartment
      moveServoTo(HOME_ANGLE + (dispenseCount + 1) * STEP_ANGLE);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(STEP_NAMES[dispenseCount]);
      lcd.setCursor(0, 1);
      lcd.print("bon app");

      dispenseCount++;
      Serial.print("Dispense #");
      Serial.println(dispenseCount);
    } else {
      // All compartments used: go back home so the separator can be refilled
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Resetting...");
      lcd.setCursor(0, 1);
      lcd.print("Servo home");

      moveServoTo(HOME_ANGLE);
      dispenseCount = 0;
      Serial.println("Reset to home position");
    }

    showDispenseMessage = true;
    dispenseMessageStart = millis();

    digitalWrite(buzzerPin, HIGH);
    buzzerActive = true;
    buzzerStartTime = millis();
    Serial.println("Buzzer ON");
  }

  // Stop the buzzer after BUZZER_DURATION_MS
  if (buzzerActive && millis() - buzzerStartTime >= BUZZER_DURATION_MS) {
    digitalWrite(buzzerPin, LOW);
    buzzerActive = false;
    Serial.println("Buzzer OFF");
  }

  // Clear the message after MESSAGE_DURATION_MS
  if (showDispenseMessage && millis() - dispenseMessageStart >= MESSAGE_DURATION_MS) {
    showDispenseMessage = false;
    lcd.clear();
  }

  delay(200);
}
