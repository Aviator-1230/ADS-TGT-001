#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int MPU_ADDR = 0x68;
const int BUZZ_PIN = 8;

float calibAltOffset = 0.0f;
float calibAzOffset  = 0.0f;
float targetAlt = NAN;
float targetAz  = NAN;

float Q_angle = 0.001, Q_bias = 0.003, R_measure = 0.03;
float angleAlt = 0, biasAlt = 0, PAlt[2][2] = {{0,0},{0,0}};
float angleAz  = 0, biasAz  = 0, PAz[2][2]  = {{0,0},{0,0}};

float kalmanUpdate(float newAngle,float newRate,float dt,
                   float &angle,float &bias,float P[2][2]) {
  float rate=newRate-bias;
  angle+=dt*rate;
  P[0][0]+=dt*(dt*P[1][1]-P[0][1]-P[1][0]+Q_angle);
  P[0][1]-=dt*P[1][1];
  P[1][0]-=dt*P[1][1];
  P[1][1]+=Q_bias*dt;
  float S=P[0][0]+R_measure;
  float K0=P[0][0]/S, K1=P[1][0]/S;
  float y=newAngle-angle;
  angle+=K0*y; bias+=K1*y;
  float P00=P[0][0], P01=P[0][1];
  P[0][0]-=K0*P00; P[0][1]-=K0*P01;
  P[1][0]-=K1*P00; P[1][1]-=K1*P01;
  return angle;
}

void setup() {
  pinMode(BUZZ_PIN, OUTPUT);
  Serial.begin(9600);
  Wire.begin();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0);
  Wire.endTransmission(true);

  if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)) {
    Serial.println("OLED fail"); while(1);
  }
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);

  // Splash screen
  display.setCursor(0,0);
  display.println("Guidance Ready");
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
  static unsigned long lastTime=millis();
  unsigned long now=millis();
  float dt=(now-lastTime)/1000.0; if(dt<=0) dt=0.001; lastTime=now;

  // Read raw MPU6050 data
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR,14,true);
  if(Wire.available()<14) { delay(10); return; }

  int16_t axRaw=Wire.read()<<8|Wire.read();
  int16_t ayRaw=Wire.read()<<8|Wire.read();
  int16_t azRaw=Wire.read()<<8|Wire.read();
  Wire.read(); Wire.read(); // skip temp
  int16_t gxRaw=Wire.read()<<8|Wire.read();
  int16_t gyRaw=Wire.read()<<8|Wire.read();
  Wire.read(); Wire.read(); // skip gz

  float axScaled=axRaw/16384.0;
  float ayScaled=ayRaw/16384.0;
  float azScaled=azRaw/16384.0;
  float gxScaled=gxRaw/131.0;
  float gyScaled=gyRaw/131.0;

  float denom=sqrt(ayScaled*ayScaled+azScaled*azScaled); if(denom==0) denom=0.0001;
  float accelAlt=atan2(axScaled,denom)*180/M_PI;
  float accelAz=atan2(ayScaled,axScaled)*180/M_PI;

  float altitude=kalmanUpdate(accelAlt,gxScaled,dt,angleAlt,biasAlt,PAlt);
  float azimuth=kalmanUpdate(accelAz,gyScaled,dt,angleAz,biasAz,PAz);

  if(isnan(altitude)) altitude=accelAlt;
  if(isnan(azimuth)) azimuth=accelAz;

  float currentAlt=altitude+calibAltOffset;
  float currentAz=azimuth+calibAzOffset;

  // --- Handle serial commands ---
  if (Serial.available()) {
    char first = Serial.peek();
    if (first == 'C') {
      // Calibration string
      String msg = Serial.readStringUntil('\n');
      msg.trim();
      if (msg.startsWith("CALIBRATE_MOON")) {
        int c1=msg.indexOf(','), c2=msg.indexOf(',',c1+1);
        if(c1>0 && c2>c1) {
          float moonAlt=msg.substring(c1+1,c2).toFloat();
          float moonAz=msg.substring(c2+1).toFloat();
          calibAltOffset=moonAlt-altitude;
          calibAzOffset=moonAz-azimuth;
          Serial.print("Offsets set: ");
          Serial.print(calibAltOffset);
          Serial.print(", ");
          Serial.println(calibAzOffset);
        }
      }
    } else if (Serial.available() >= 8) {
      // Binary float target
      byte buf[8];
      Serial.readBytes(buf, 8);
      float tAlt, tAz;
      memcpy(&tAlt, &buf[0], 4);
      memcpy(&tAz, &buf[4], 4);
      targetAlt = tAlt;
      targetAz  = tAz;
      Serial.print("Target set (binary): ");
      Serial.print(targetAlt);
      Serial.print(", ");
      Serial.println(targetAz);
    }
  }

  // OLED output
  static unsigned long lastOLED=0;
  if(millis()-lastOLED>200) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Alt: "); display.println(currentAlt,1);
    display.print("Az:  "); display.println(currentAz,1);

    if(!isnan(targetAlt) && !isnan(targetAz)) {
      display.print("Target Alt: "); display.println(targetAlt,1);
      display.print("Target Az:  "); display.println(targetAz,1);
      if(fabs(currentAlt-targetAlt)<2 && fabs(currentAz-targetAz)<2) {
        display.println("Aligned!"); tone(BUZZ_PIN,1000,200);
      } else {
        if(currentAlt<targetAlt) display.println("Move Up");
        else display.println("Move Down");
        if(currentAz<targetAz) display.println("Move Right");
        else display.println("Move Left");
      }
    } else {
      display.println("No target set");
    }

    display.display();
    lastOLED=millis();
  }
}
