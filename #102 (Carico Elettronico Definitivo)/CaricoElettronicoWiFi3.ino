#include <WiFiManager.h>
#include <Wire.h>
//#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Arduino.h>
#include <AiEsp32RotaryEncoder.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>

#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define Switch D3
#define WiFiLed D4
#define ONE_WIRE_BUS D5
#define R_CLK D7
#define R_DT D6
#define setAmper D8

#define ROTARY_ENCODER_STEPS 4

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(R_CLK, R_DT, Switch, -1, ROTARY_ENCODER_STEPS);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_ADS1115 ads;

WiFiManager wm;
//int sensorValue = 0;
int _amper = 0;
unsigned long analogLoop = millis();
unsigned long refreshLoop = millis();
unsigned long tempLoop = millis();

bool _menuSelected = false;
bool _active = false;
unsigned int _menu = 0;
int _menuSize = 2;
int serialInput;
int encoderPosition;
float tempC;
float _cutOff = 0.00;
float volts0, volts1, volts2;

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SCREEN_ADDRESS, true)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.display();
  display.clearDisplay();
  display.setTextSize(1);              // Normal 1:1 pixel scale
  display.setTextColor(SH110X_WHITE);  // Draw white text
  display.setCursor(0, 0);
  delay(500);  // Start at top-left corner
  display.println(F("Init Display..."));
  display.display();
  delay(500);
  pinMode(setAmper, OUTPUT);
  pinMode(WiFiLed, OUTPUT);
  ads.setGain(GAIN_ONE);
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1)
      ;
  }
  display.println(F("Init I/O..."));
  display.display();
  delay(500);
  digitalWrite(WiFiLed, LOW);
  display.println(F("Init WiFi..."));
  display.display();
  delay(500);
  WiFi.mode(WIFI_STA);
  delay(500);
  display.println(F("Init Temp Sensor..."));
  display.display();
  sensors.begin();
  delay(500);
  display.println(F("Init Encoder..."));
  display.display();
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(0, _menuSize, false);
  rotaryEncoder.setAcceleration(250);
  delay(500);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(60);
  // wm.resetSettings();

  if (wm.autoConnect("AutoConnectAP")) {
    Serial.println("connected...yeey :)");
  } else {
    Serial.println("Configportal running");
  }
}

void loop() {
  int16_t adc0, adc1, adc2;
  wm.process();
  rotary_loop();

  if (millis() - tempLoop > 10000) {
    sensors.requestTemperatures();
    tempC = sensors.getTempCByIndex(0);
    tempLoop = millis();
  }

  if (millis() - refreshLoop > 100) {
    refreshScreen();
    refreshLoop = millis();
  }

  if (millis() - analogLoop > 500) {
    adc0 = ads.readADC_SingleEnded(0);
    adc1 = ads.readADC_SingleEnded(1);
    adc2 = ads.readADC_SingleEnded(2);
    volts0 = ads.computeVolts(adc0);
    volts1 = ads.computeVolts(adc1);
    volts2 = ads.computeVolts(adc2);
    analogLoop = millis();
  }

  if(_active){
    int Amper = map(_amper, 0, 25400, 0, 254);
    analogWrite(setAmper, Amper);
  }else{
    analogWrite(setAmper, 0);
  }

  if (Serial.available() > 0) {
    static char input[16];
    static uint8_t i;
    char c = Serial.read();
    if (c != '\n' && i < 15)  // assuming "Carriage Return" is chosen in the Serial monitor as the line ending character
      input[i++] = c;
    else {
      input[i] = '\0';
      i = 0;
      _amper = atoi(input);
      if (_amper >= 0 and _amper < 20000) {
        serialInput = _amper;
        int Amper = map(_amper, 0, 20000, 0, 254);
        //analogWrite(setAmper, Amper);
        Serial.print("A set: ");
        Serial.println(Amper);
      }
      Serial.println(serialInput);
    }
  }
}


void refreshScreen() {
  display.clearDisplay();
  display.setTextSize(1);              // Normal 1:1 pixel scale
  display.setTextColor(SH110X_WHITE);  // Draw white text
  display.setCursor(10, 0);
  display.print("Temp Diss: ");
  display.print(tempC);
  display.print(" C");
  display.setCursor(10, 10);
  display.print("Load Volt: ");
  display.print(volts0 * 11.0222312);
  display.print(" V");
  display.setCursor(10, 20);
  display.print("Amper: ");
  display.print((volts1 + volts2)*10);
  display.print(" A");
  display.setCursor(10, 30);
  display.print("Cut OFF: ");
  display.print(_cutOff);
  display.print(" V");
  display.setCursor(10, 40);
  display.print("Amper SET: ");
  display.print(_amper);
  display.print(" mA");
  display.setCursor(10, 50);
  if (_active) display.print("ACTIVATED");
  else display.print("DEACTIVATED");


  switch (_menu) {
    case 0:
      display.setCursor(0, 30);
      display.print(_menuSelected ? "-" : ">");
      break;
    case 1:
      display.setCursor(0, 40);
      display.print(_menuSelected ? "-" : ">");
      break;
    case 2:
      display.setCursor(0, 50);
      display.print(_menuSelected ? "-" : ">");
      break;
  }
  display.display();
}

void rotary_loop() {
  static bool _first = false;
  if (rotaryEncoder.encoderChanged()) {
    if (!_menuSelected) {
      rotaryEncoder.setBoundaries(0, 2, false);
      if (_first) {
        rotaryEncoder.setEncoderValue(_menu);
        _first = false;
      }
      _menu = rotaryEncoder.readEncoder();
      Serial.println(_menu);
    } else {
      switch (_menu) {
        case 0:
          rotaryEncoder.setBoundaries(0, 5000, false);
          if (_first) {
            rotaryEncoder.setEncoderValue(_cutOff*100);
            _first = false;
          }
          _cutOff = (float)rotaryEncoder.readEncoder() / 100.00;
          break;
        case 1:
          rotaryEncoder.setBoundaries(0, 20000, false);
          if (_first) {
            rotaryEncoder.setEncoderValue(_amper);
            _first = false;
          }
          _amper = rotaryEncoder.readEncoder();
          break;
        case 2:
          _active = !_active;
          break;
      }
    }
  }
  if (rotaryEncoder.isEncoderButtonClicked()) {
    _menuSelected = !_menuSelected;
    _first = true;
    delay(50);
  }
}
