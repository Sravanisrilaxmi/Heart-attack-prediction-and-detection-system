#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ---------- OLED Settings ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- MAX30102 ----------
MAX30105 max30102;
const uint16_t MY_BUFFER_SIZE = 100;
uint32_t irBuffer[MY_BUFFER_SIZE];
uint32_t redBuffer[MY_BUFFER_SIZE];
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHR;

// ---------- Pins ----------
#define BTN_AGE1 25
#define BTN_AGE2 26
#define BTN_AGE3 27
#define RED_LED 32
#define GREEN_LED 33
#define BUZZER 14

int ageGroup = -1;
bool measuring = false;

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL);

  // Buttons
  pinMode(BTN_AGE1, INPUT_PULLUP);
  pinMode(BTN_AGE2, INPUT_PULLUP);
  pinMode(BTN_AGE3, INPUT_PULLUP);

  // LEDs & buzzer
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // OLED
  if(!display.begin(0x3C, true)){
    Serial.println("OLED not found!");
    while(1);
  }
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  // MAX30102 init
  if(!max30102.begin(Wire)){
    display.clearDisplay();
    display.setCursor(0,10);
    display.println("MAX30102 not found!");
    display.display();
    while(1);
  }
  max30102.setup();
  max30102.setPulseAmplitudeRed(0x0A);
  max30102.setPulseAmplitudeIR(0x0A);
  max30102.setPulseAmplitudeGreen(0x00);

  showAgeMenu();
}

// ---------- Loop ----------
void loop() {
  // Age selection
  if(!measuring){
    if(digitalRead(BTN_AGE1) == LOW){ ageGroup=0; startMeasurement(); }
    else if(digitalRead(BTN_AGE2) == LOW){ ageGroup=1; startMeasurement(); }
    else if(digitalRead(BTN_AGE3) == LOW){ ageGroup=2; startMeasurement(); }
  }

  if(measuring){
    // Collect data
    for(int i=0;i<MY_BUFFER_SIZE;i++){
      while(!max30102.check());
      redBuffer[i] = max30102.getRed();
      irBuffer[i] = max30102.getIR();
    }

    // Compute HR & SpO2
    maxim_heart_rate_and_oxygen_saturation(irBuffer, MY_BUFFER_SIZE,
                                           redBuffer, &spo2, &validSPO2,
                                           &heartRate, &validHR);

    // Display data
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Measuring...");
    display.setCursor(0,15);
    display.print("HR: "); display.print(validHR?heartRate:0); display.println(" BPM");
    display.print("SpO2: "); display.print(validSPO2?spo2:0); display.println(" %");

    display.setCursor(0,45);
    display.print("Age Group: ");
    if(ageGroup==0) display.println("below 11 months");
    else if(ageGroup==1) display.println("1-9 yrs");
    else display.println("above 9 yrs");

    int condition = classifyCondition(heartRate, spo2, ageGroup);
    display.setCursor(0,55);

    // ----- CORRECTED LOGIC -----
    if(condition == 2){ // 2 = DANGER
      display.println("DANGER!");
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
      digitalWrite(BUZZER, HIGH);
    } 
    else if(condition == 1){ // 1 = MODERATE
      display.println("MODERATE");
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, LOW);
      digitalWrite(BUZZER, LOW);
    } 
    else { // 0 = SAFE
      display.println("SAFE");
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      digitalWrite(BUZZER, LOW);
    }
    // --------------------------

    display.display();
    delay(1000);
  }
}

// ---------- Functions ----------
void showAgeMenu(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Select Age Group:");
  display.println("BTN1:below 11 months");
  display.println("BTN2:1-9 yrs");
  display.println("BTN3:above 9 yrs");
  display.display();
}

void startMeasurement(){
  measuring = true;
  display.clearDisplay();
  display.setCursor(0,10);
  display.println("Starting measurement...");
  display.display();
  delay(1000);
}

int classifyCondition(double hr, double spo2, int ageGroup){
  if(spo2<90) return 2;
  else if(spo2>=90 && spo2<94) return 1;

  switch(ageGroup){
    case 0://<11months
      if(hr<50 || hr>130) return 2;
      else if((hr>=50 && hr<55) || (hr>125 && hr<=130)) return 1;
      else return 0;
    case 1://1-9 yrs
      if(hr<50 || hr>120) return 2;
      else if((hr>=50 && hr<55) || (hr>115 && hr<=120)) return 1;
      else return 0;
    case 2://above 9 yrs
      if(hr<45 || hr>110) return 2;
      else if((hr>=45 && hr<50) || (hr>105 && hr<=110)) return 1;
      else return 0;
    default: return 0;
  }
}