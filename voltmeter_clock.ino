// Arduino IDE version 1.8.15
// AVR board version 1.8.3
#include <Encoder.h>    // 1.4.2
#include <Wire.h>   // built-in
#include <DS3231.h> // 1.0.7
#include <FastLED.h>  // 3.004.000, used for EVERY_N_MILLISECONDS and applyGamma_video()
#include <AceButton.h>  // 1.9.1
using namespace ace_button;

// Settings
#define SECOND_PIN 11
#define MINUTE_PIN 10
#define HOUR_PIN 9
#define SECOND_LIGHT_PIN 6
#define MINUTE_LIGHT_PIN 5
#define HOUR_LIGHT_PIN 3
#define BUTTON_PIN 2
#define ENTER_SETUP_MS 1000  // duration to hold button to enter timeset
#define SET_METER_MS 300     // duration of press to set a meter
#define ENCODER_PULSE_PER_DETENT 4

AceButton button(BUTTON_PIN, HIGH);
// Forward reference to prevent Arduino compiler becoming confused.
void handleButton(AceButton*, uint8_t, uint8_t);
bool set_mode = false;
byte selected_meter = 0;    // hours = 0, minutes = 1, seconds = 2

Encoder encoder(7, 8);
long old_position = 0;
byte light_intensity = 255;

DS3231 clock;
bool h12Flag, pmFlag = false;

// time registers
byte seconds = 0;
byte minutes = 0;
byte hours = 0;

float hours_lut[] = {
    0, 0,
    6, 62,
    12, 124,
    18, 188,
    24, 255
};

float minutes_lut[] = {
    0, 0,
    5, 21,
    10, 45,
    15, 66,
    20, 87,
    25, 108,
    30, 129,
    35, 150,
    40, 172,
    45, 193,
    50, 213, // known good
    55, 234, // gets sticky here
    60, 255  // known good
};

float seconds_lut[] = {
    0, 0,
    5, 21,
    10, 42,
    15, 63,
    20, 84,
    25, 105,
    30, 126, // sticky here
    35, 147,
    40, 168,
    45, 190,
    50, 212, // known good
    55, 232, // gets sticky here
    60, 255  // known good
};


void setup () {
    pinMode(SECOND_PIN, OUTPUT);
    pinMode(MINUTE_PIN, OUTPUT);
    pinMode(HOUR_PIN, OUTPUT);
    pinMode(SECOND_LIGHT_PIN, OUTPUT);
    pinMode(MINUTE_LIGHT_PIN, OUTPUT);
    pinMode(HOUR_LIGHT_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    ButtonConfig* buttonConfig = button.getButtonConfig();
    buttonConfig->setEventHandler(handleButton);
    buttonConfig->setFeature(ButtonConfig::kFeatureClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    buttonConfig->setClickDelay(SET_METER_MS);
    buttonConfig->setLongPressDelay(ENTER_SETUP_MS);

    Wire.begin();
    update_lights();
}


void loop () {
    EVERY_N_MILLISECONDS(5) {
        button.check();
        read_encoder();
    }

    if (set_mode) {
        EVERY_N_MILLISECONDS(100) {
            display();
        }
    } else {
        EVERY_N_MILLISECONDS(1000) {
            update_from_clock();
            display();
        }
    }
}


void update_from_clock() {
    hours = clock.getHour(h12Flag, pmFlag);
    minutes = clock.getMinute();
    seconds = clock.getSecond();
}


void display() {
    analogWrite(HOUR_PIN, meter_map(hours, hours_lut, 10));         //convert from 0-24 to 0-255
    analogWrite(MINUTE_PIN, meter_map(minutes, minutes_lut, 26));   //convert from 0-60 to 0-255
    analogWrite(SECOND_PIN, meter_map(seconds, seconds_lut, 26));   //convert from 0-60 to 0-255
}


void set_clock() {
    clock.setClockMode(false);    // set to 24h
    clock.setHour(hours);
    clock.setMinute(minutes);
    clock.setSecond(seconds);
}


void handleButton(AceButton* /* button */, uint8_t eventType, uint8_t buttonState) {
    switch (eventType) {
        case AceButton::kEventLongPressed:
            if (set_mode) {
                set_clock();
            }
            set_mode = !set_mode;
            update_lights();
            break;
        case AceButton::kEventClicked:
            if (set_mode) {
                selected_meter = (selected_meter + 1) % 3;
                update_lights();
            }
            break;
    }
}


void read_encoder() {
    long newPosition = encoder.read();
    if (newPosition != old_position) {
        if (newPosition - old_position >= ENCODER_PULSE_PER_DETENT) {
            handleEncoder(1);
            old_position = newPosition;
        } else if (old_position - newPosition >= ENCODER_PULSE_PER_DETENT) {
            handleEncoder(-1);
            old_position = newPosition;
        }
    }
}


void handleEncoder(int amount) {
    if (set_mode) {
        switch (selected_meter) {
            case 0:
                hours = constrain((hours + amount), 0, 23);
                break;
            case 1:
                minutes = constrain((minutes + amount), 0, 59);
                break;
            case 2:
                seconds = constrain((seconds + amount), 0, 59);
                break;
        }
    } else {
        light_intensity = constrain((light_intensity + amount * 10), 0, 255); //0-255 inclusive
        update_lights();
    }
}


void update_lights() {
    byte light_values[] = {0, 0, 0};

    byte gamma_light_intensity = applyGamma_video(light_intensity, 2.4);
    
    if (set_mode) {
        switch (selected_meter) {
            case 0:
                light_values[0] = gamma_light_intensity;
                break;
            case 1:
                light_values[1] = gamma_light_intensity;
                break;
            case 2:
                light_values[2] = gamma_light_intensity;
                break;
        }
    } else {
        light_values[0] = gamma_light_intensity;
        light_values[1] = gamma_light_intensity;
        light_values[2] = gamma_light_intensity;
    }
    analogWrite(HOUR_LIGHT_PIN, light_values[0]);
    analogWrite(MINUTE_LIGHT_PIN, light_values[1]);
    analogWrite(SECOND_LIGHT_PIN, light_values[2]);
}


// lookup table lerping
byte meter_map(byte value, float lut[], byte lut_length) {
    for (int i = 0; i < lut_length - 2; i = i + 2) {
        if ((value >= lut[i]) && (value <= lut[i + 2])) {
            return lut[i + 1] - ((lut[i + 1] - lut[i + 3]) * ((value - lut[i]) / (lut[i + 2] - lut[i])));
        }
    }
}
