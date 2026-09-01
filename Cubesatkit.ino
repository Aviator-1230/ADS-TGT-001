#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUZZER_PIN 8

Adafruit_MPU6050 mpu;

float moonOffsetAlt = 0, moonOffsetAz = 0;
float currentAlt = 0, currentAz = 0;
float targetAlt = 0, targetAz = 0;

float alpha = 0.98;   // complementary filter weight
float dt = 0.05;      // loop time (20 Hz)

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  pinMode(BUZZER_PIN, OUTPUT);

 /*if (!mpu.begin()) {
    Serial.println("MPU6050 not found");
    while (true);
  }*/
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Load Moon offset from EEPROM
  EEPROM.get(0, moonOffsetAlt);
  EEPROM.get(sizeof(float), moonOffsetAz);

  Serial.println("System ready");
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Estimate orientation from accelerometer
  float accelAlt = atan2(accel.acceleration.y, accel.acceleration.z) * 180 / PI;
  float accelAz  = atan2(accel.acceleration.x, accel.acceleration.z) * 180 / PI;

  // Complementary filter (gyro integration + accel correction)
  currentAlt = alpha * (currentAlt + gyro.gyro.x * dt) + (1 - alpha) * accelAlt;
  currentAz  = alpha * (currentAz + gyro.gyro.y * dt) + (1 - alpha) * accelAz;

  // Apply Moon offset
  currentAlt += moonOffsetAlt;
  currentAz  += moonOffsetAz;

  // Handle serial input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    if (input.startsWith("CALIBRATE_MOON")) {
      int firstComma = input.indexOf(',');
      int secondComma = input.indexOf(',', firstComma + 1);
      float moonAlt = input.substring(firstComma + 1, secondComma).toFloat();
      float moonAz  = input.substring(secondComma + 1).toFloat();

      moonOffsetAlt = moonAlt - currentAlt;
      moonOffsetAz  = moonAz - currentAz;

      EEPROM.put(0, moonOffsetAlt);
      EEPROM.put(sizeof(float), moonOffsetAz);

      Serial.println("Moon calibration stored");
    } else {
      int comma = input.indexOf(',');
      targetAlt = input.substring(0, comma).toFloat();
      targetAz  = input.substring(comma + 1).toFloat();
    }
  }

  // Guidance logic
  float dAlt = targetAlt - currentAlt;
  float dAz  = targetAz - currentAz;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Alt Err: "); display.println(dAlt, 1);
  display.print("Az Err: ");  display.println(dAz, 1);

  if (abs(dAlt) < 1 && abs(dAz) < 1) {
    display.println("Aligned!");
    tone(BUZZER_PIN, 1000); // steady tone
  } else {
    if (dAlt > 1) display.println("Move Up");
    else if (dAlt < -1) display.println("Move Down");

    if (dAz > 1) display.println("Move Right");
    else if (dAz < -1) display.println("Move Left");

    noTone(BUZZER_PIN);
    tone(BUZZER_PIN, 500, 200); // short beep
  }

  display.display();
  delay(50);
}
