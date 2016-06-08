// Default code for Arduino board from Netflix using IR pulses for a Sony Televison

// ======================== LIBS ==============================

#include <avr/power.h>
#include <avr/sleep.h>
#include <Adafruit_NeoPixel.h> 

// ======================== PINS ==============================

//neopixel
#define INDICATOR_PIN 8
#define SOFT_SWITCH_PIN 3

//accelerometer
#define x A3
#define y A4
#define z A5

#define IR_PIN     4
#define NUM_PULSES 60
int pulse_widths[NUM_PULSES][2] = {{4716,2416},{572,628},{568,628},{572,616},{580,1224},{572,1224},{568,620},{580,620},{572,1220},{576,1228},{568,1224},{572,616},{580,1224},{572,620},{576,620},{576,1220},{20308,2420},{572,628},{568,620},{576,624},{572,1220},{576,1220},{576,620},{576,624},{572,1220},{576,1220},{572,1220},{576,624},{572,1220},{576,624},{572,620},{576,1224},{20304,2416},{572,620},{576,620},{576,624},{572,1224},{572,1220},{576,624},{572,628},{568,1224},{568,1224},{572,1224},{572,624},{572,1224},{568,628},{572,620},{576,1216},{20360,2420},{568,620},{576,624},{572,628},{568,1224},{572,1224},{572,624},{568,624},{576,1216},{576,1216},{580,1224},{572,620}};

// ======================== VARS ==============================
Adafruit_NeoPixel indicator = Adafruit_NeoPixel(1, INDICATOR_PIN);
uint8_t colors[3][3] = {{1, 1, 1}, {1, 1, 1}, {1, 0, 0}}; 
uint8_t colorState = 255;
uint8_t colorPulseIncrement = -1;
volatile bool cpuSleepFlag = true;
unsigned long nextReadTime, windowTime, cpuAwoken, userReallyAsleepStart, indicatorPulseTime;
int indicatorPulseDelay = 5; 
int readDelay = 50;
int windowDelay = 1500; 
unsigned long userReallyAsleepDelay = 60000UL; 
int threshold = 50; 
int consecutiveThresholdTime = 60;
int userSleepState = 0; 
bool userReallyAsleep = false;
bool newWindow = true;
int pxVal = 0;
int pyVal = 0;
int pzVal = 0;
int movementSum = 0; 
int consecutivePossibleSleeps = 0; 

// ======================== SETUP ==============================
void setup() 
{
  analogReference(EXTERNAL); 
  pinMode(SOFT_SWITCH_PIN, INPUT);
  pinMode(IR_PIN, OUTPUT);
  digitalWrite(IR_PIN, LOW);
  nextReadTime = millis();
  windowTime = nextReadTime;
  indicator.begin();
  indicator.setBrightness(50); 
  cpuSleepNow();
}

// ======================== LOOP ==============================
void loop() {
  
  //everthing broken up into seperate functions to keep things simple.
  if(millis() - indicatorPulseTime > indicatorPulseDelay){
    indicatorPulseTime = millis();
    indicatorHandler();
  }
  softSwitchHandler();
  sleepHandler();
  irHandler();
  
  //new calculation window every windowDelay
  if (millis() - windowTime > windowDelay) {
    windowTime = millis();
    newWindow = true;
  }
  
  //read accelerometer every readDelay amount of time
  if (millis() - nextReadTime > readDelay) {
    nextReadTime = millis();
    accelerometerHandler();
  }
}

// ======================== SOFT SWITCH ==============================
void softSwitchHandler(){
  if(digitalRead(SOFT_SWITCH_PIN) == LOW && millis()-cpuAwoken > 1000){
    //its held down. 
    cpuSleepNow();
  }
}

// ======================== ACCELEROMETER ==============================
void accelerometerHandler() {
  //read the accelerometer
  int xVal = analogRead(x);
  int yVal = analogRead(y);
  int zVal = analogRead(z);
  
  //if its a new calculation window
  if (newWindow) {
    //if the displacement is less than the threshold, then the user may be asleep.
    if (movementSum < threshold) {
      if(userSleepState !=2){
        userSleepState = 1;  
      }
    } else {
      userSleepState = 0;
    }

    if (userSleepState >= 1) {
      consecutivePossibleSleeps++;
      if (consecutivePossibleSleeps > (consecutiveThresholdTime * ((float)1000/windowDelay))){
        userSleepState = 2;
      }
    } else {
      consecutivePossibleSleeps = 0;
    }

    pxVal = xVal;
    pyVal = yVal;
    pzVal = zVal;
    movementSum = 0;
    newWindow = false;
  } else {
    //compute magnitude changes
    movementSum += abs(xVal - pxVal) + abs(yVal - pyVal) + abs(zVal - pzVal);
  }
}
// ======================== USER SLEEP ==============================
void sleepHandler(){
  if(userSleepState == 2){
    if(millis() - userReallyAsleepStart > userReallyAsleepDelay){
      userReallyAsleep = true;
    }
  }else{
    userReallyAsleepStart = millis();
  }
}

// ======================== INDICATOR ==============================
void indicatorHandler(){
  //pulse the indicator for whichever color it is. can change what the pulse looks like here. 
  colorState+=colorPulseIncrement;
  if(colorState < 1){
    colorPulseIncrement = 1;
  }else if(colorState >254){
    colorPulseIncrement = -1;
  }
  indicator.setPixelColor(0,colorState * colors[userSleepState][0], colorState * colors[userSleepState][1], colorState * colors[userSleepState][2]);
  indicator.show();
}

// ======================== CPU SLEEP ==============================
void cpuSleepNow() {  
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
    sleep_enable();
    delay(100);
    //turn stuff off
    indicator.setPixelColor(0,0,0,0);
    indicator.show();
    attachInterrupt(digitalPinToInterrupt(3),pinInterrupt, FALLING);
    sleep_mode();
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(3));
}  


void pinInterrupt()  
{  
    cpuAwoken = millis();
    reSetup();
}  

//when the microcontroller is woken up, lets reset values
void reSetup(){
  userReallyAsleep = false;
  userSleepState = 0;
}


// ======================== IR ==============================
void irHandler(){
  if(userReallyAsleep){
    //shut down the indicator
    indicator.setPixelColor(0,0,0,0);
    indicator.show();
    IR_transmit_pwr();
    //repeat if desired. or add any other codes syou may want to handle. This is whatever your TV needs.
    delay(500);
    IR_transmit_pwr();
    //put the microcontrolelr to sleep
    cpuSleepNow();
  }
}
//based on src from https://learn.adafruit.com/ir-sensor
void pulseIR(long microsecs) {
  while (microsecs > 0) {
    // 38 kHz is about 13 microseconds high and 13 microseconds low
    digitalWrite(IR_PIN, HIGH);  // this takes about 3 microseconds to happen
    delayMicroseconds(10);         // hang out for 10 microseconds
    digitalWrite(IR_PIN, LOW);   // this also takes about 3 microseconds
    delayMicroseconds(10);         // hang out for 10 microseconds
    // so 26 microseconds altogether
    microsecs -= 26;
  }
}

void IR_transmit_pwr() {
  for (int i = 0; i < NUM_PULSES; i++) {
    delayMicroseconds(pulse_widths[i][0]);
    pulseIR(pulse_widths[i][1]);
  }
}
