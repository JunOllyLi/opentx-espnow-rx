/* 
 * This file is part of the opentx-espnow-rx (https://github.com/tmertelj/opentx-espnow-rx).
 * Copyright (c) 2019 Tomaz Mertelj.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <Arduino.h>
#include "esprx.h"

#define US_TICKS  5
#define POLARITY 1

#if defined(ESP8266)
#define IRAM_ATTR
#elif defined(ESP32)
static hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#endif

static int channel_pins[] = {
    35, 36, 9, 7, 8, 37
};

#define USED_OUTPUT_CHANNELS (sizeof(channel_pins)/sizeof(channel_pins[0]))

enum PWMState_t {
  PWM_UNINIT,
  PWM_DISABLED,
  PWM_ENABLED,
};

static volatile uint32_t pulses[USED_OUTPUT_CHANNELS + 1];
static uint16_t current_channel = 0U;

static int frameLength = 22; //22 ms 
static volatile PWMState_t pwmState = PWM_UNINIT;

IRAM_ATTR void setupPulsesPWM(int channelsStart)
{
  static uint32_t PWM_min = 1000 * US_TICKS;
  static uint32_t PWM_max = 2000 * US_TICKS;  
  static uint32_t PWM_offs = 1500 * US_TICKS;

  unsigned int firstCh = channelsStart;
  unsigned int lastCh = firstCh + 8;
  lastCh = lastCh > USED_OUTPUT_CHANNELS ? USED_OUTPUT_CHANNELS : lastCh;

  volatile uint32_t *ptr = pulses;
  uint32_t rest = frameLength * 1000 * US_TICKS;
  for (int i=firstCh; i<lastCh; i++) {
    uint32_t v = (locChannelOutputs[i] * US_TICKS)/2 + PWM_offs;
    v = v < PWM_min ? PWM_min : v;
    v = v > PWM_max ? PWM_max : v;
    rest -= v;
    *ptr++ = v;
  }
  *ptr = rest;
}


static IRAM_ATTR void timer_callback()
{
  static bool intEn = true;
#if defined(ESP8266)
  noInterrupts();
#elif defined(ESP32)
  portENTER_CRITICAL_ISR(&timerMux);
#endif
  if (PWM_ENABLED == pwmState){
    if (0U == current_channel) {
      setupPulsesPWM(0);
    } else {
       // stop previous channel
       digitalWrite(channel_pins[current_channel - 1], !POLARITY);
    }
    if (current_channel < USED_OUTPUT_CHANNELS) {
      // start next channel
      digitalWrite(channel_pins[current_channel], POLARITY);
    }
  } else {
    for (int i = 0; i < USED_OUTPUT_CHANNELS; i++) {
      digitalWrite(channel_pins[i], !POLARITY);  // stop all channels
    }
  }
  intEn = PWM_ENABLED == pwmState;

  uint32_t pulse = pulses[current_channel];
  current_channel++;
  if (current_channel == USED_OUTPUT_CHANNELS + 1) { // last channel is the rest period
    current_channel = 0;
  }

#if defined(ESP8266)
  timer1_write(pulse);
  interrupts();
#elif defined(ESP32)
  if (intEn) {
    timerAlarmWrite(timer, pulse, true);
  } 
  else {
    timerStop(timer);
  }
  portEXIT_CRITICAL_ISR(&timerMux);
#endif
}

void initOUTPUT() {
  for (int i = 0; i < USED_OUTPUT_CHANNELS; i++) {
    pinMode(channel_pins[i], OUTPUT);
    digitalWrite(channel_pins[i], !POLARITY);
  }
  
#if defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(timer_callback);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
#elif defined(ESP32)
  timer = timerBegin(0, 16, true); // 5 MHz
  timerAttachInterrupt(timer, timer_callback, true);
#endif
  pwmState = PWM_ENABLED;

  timer_callback();
#if defined(ESP32)  
  timerAlarmEnable(timer);
#endif  
}

void disableOUTPUT(){
  if (PWM_ENABLED == pwmState) {
    pwmState = PWM_DISABLED;
    delay(2*frameLength);
  }
}

void enableOUTPUT(){
  if (PWM_DISABLED == pwmState) {
    pwmState = PWM_ENABLED;
#if defined(ESP32)  
    timerRestart(timer);
#endif  
  }
}
