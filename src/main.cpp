
#define FASTLED_ALLOW_INTERRUPTS 1

#include <Arduino.h>
#include <FastLED.h>
#include <avr/sleep.h>
#include <EEPROM.h>

/** Debug mode **/
/* If enabled will sleep after N minutes, instead of N hours */
// #define DEBUG_MODE

#define NUM_LEDS 18      /* The amount of pixels/leds you have */
#define DATA_PIN 6       /* The pin your data line is connected to */
#define LED_TYPE WS2812B /* LED strip type */
#define COLOR_ORDER GRB
#define BRIGHTNESS 32    /* the brightness of the leds (0-255) */
#define BUTTON_PIN 2     /* input pin for the button */
#define EEPROM_ADDR_SLEEP 0   /* EEPROM slot to store sleep time preference */

/**
 * START Configurable options
 */
uint16_t LONG_PRESS_TICKS = 700; /* how many ticks must pass to consider a button-press "long" */
uint8_t colourChangeSpeed = 3;   /* speed of colour change */
uint8_t deltaHue = 3;            /* amount of colour change between each LED */
uint8_t defaultSleepHours = 1;   /* amount of colour change between each LED */
uint8_t maxSleepHours = 4;       /* amount of colour change between each LED */
/**
 * END Configurables
 */


/** internal registers **/
CRGB leds[NUM_LEDS];
uint8_t sleepHours;
uint8_t currentHue = 0;
uint16_t buttonHoldCount = 0;
bool buttonState = HIGH;
bool longPress = false;
bool ignorePress = false;
bool ledToggle = false;
volatile bool poweredOn = true;
unsigned long previousMillis=0;

void writeSleepHours() {    
    EEPROM.write(EEPROM_ADDR_SLEEP, sleepHours);
}

void initSleepHours() {    
    uint8_t eepromValue = EEPROM.read(EEPROM_ADDR_SLEEP);

    if (eepromValue > maxSleepHours || eepromValue < 1) {
        sleepHours = defaultSleepHours;
    } else { 
        sleepHours = eepromValue;
    }

    writeSleepHours();
}


void setup() {
    // setup button input
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // read sleep prefs from EEPROM
    initSleepHours();

    // setup LEDs
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    
    digitalWrite(LED_BUILTIN, HIGH);
}

void resetButtonState() {
    longPress = false;
    ignorePress = false;
    buttonHoldCount = 0;
}

void updateColours(uint8_t offset = 0) {
    currentHue = beatsin8(colourChangeSpeed, 0,255, 255);
    fill_rainbow(leds, NUM_LEDS, currentHue, deltaHue); 
}

void resetTimer() {
    previousMillis = millis();
}

void checkSleepTimer() {
    unsigned long currentMillis = millis();
    unsigned long interval = sleepHours * (1000UL * 60UL * 60UL);  // 1000ms (1s) x 60 sec (1m) * 60 min (1h)
    
    #ifdef DEBUG_MODE
    // testing = use mins instead of hours
    interval = sleepHours * (1000UL * 60UL); // 1000ms (1s) x 60 sec (1m)
    #endif

    if ((unsigned long)(currentMillis - previousMillis) >= interval) {
        poweredOn = false;
        resetTimer();
    } else {
        poweredOn = true;
    }
}

void fadeIn() {
    for (int i = 0; i <= BRIGHTNESS; i++) {
        FastLED.setBrightness(i);
        FastLED.delay(2);
    }
}

void fadeOut() {
    for (int i = BRIGHTNESS; i >= 0; i--) {
        FastLED.setBrightness(i);
        FastLED.delay(2);
    }
}

void flashLeds(int count) {
    
    fadeOut();

    delay(500);

    // flash <count> times
    for (int i = 0; i < count; i++) {
        
        // Flash on
        updateColours();
        FastLED.setBrightness(BRIGHTNESS);
        FastLED.show(BRIGHTNESS);
        delay(250);
        
        // Flash off
        updateColours();
        FastLED.setBrightness(0);
        FastLED.show(0);
        delay(300);
    }

    delay(300);

    fadeIn();
    
}

void incrementSleepClock() {
    sleepHours++;
    if (sleepHours > maxSleepHours) {
        sleepHours = 1; 
    }
    writeSleepHours();
    flashLeds(sleepHours);
}

void wakeUp() {
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));

    digitalWrite(LED_BUILTIN, HIGH);
    fadeIn();

    ignorePress = true;
}

void powerDown() {

    fadeOut();
    digitalWrite(LED_BUILTIN, LOW);

    delay(2000); // pause to prevent an immediate wake-up if button held

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    if (!poweredOn) {
        sleep_enable();
        sleep_bod_disable();
        attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), wakeUp, LOW);
        sei();
        sleep_cpu();
    }
    sei();
}

void loop() {
    checkSleepTimer();

    updateColours();
    buttonState = digitalRead(BUTTON_PIN);

    if (buttonState == LOW) { // button held
        buttonHoldCount++;

        if (!longPress && buttonHoldCount >= LONG_PRESS_TICKS) { // long-press
            longPress = true;
            poweredOn = false; // power down
        }

    } else { // button released

        if (buttonHoldCount > 0 && !longPress && !ignorePress) {  // short-press   
            incrementSleepClock();
        }

        resetButtonState();
    }

    // power-down after a long-press
    if (!poweredOn) {
        powerDown();
    } else {
        FastLED.setBrightness(BRIGHTNESS);           
    }

    FastLED.show();
}
