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

#include "sdkconfig.h"
#include <Arduino.h>
#include "esp_system.h" //This inclusion configures the peripherals in the ESP system.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/mcpwm.h"
////
// for chips that had MCPWM. Not tested yet.
////

#define TAG "MCPWM"

#define NUM_CHANNELS 6
typedef struct {
	mcpwm_unit_t unit;
	mcpwm_timer_t timer;
	mcpwm_generator_t operator;
	mcpwm_io_signals_t io;
	gpio_num_t gpio;
} pwm_map_t;

const pwm_map_t pwm_map[NUM_CHANNELS] = {
	{.unit = MCPWM_UNIT_0, .timer = MCPWM_TIMER_0, .operator = MCPWM_GEN_A, .io = MCPWM0A, .gpio = GPIO_NUM_37},
	{.unit = MCPWM_UNIT_0, .timer = MCPWM_TIMER_1, .operator = MCPWM_GEN_A, .io = MCPWM1A, .gpio = GPIO_NUM_36},
	{.unit = MCPWM_UNIT_0, .timer = MCPWM_TIMER_2, .operator = MCPWM_GEN_A, .io = MCPWM2A, .gpio = GPIO_NUM_35},
	{.unit = MCPWM_UNIT_1, .timer = MCPWM_TIMER_0, .operator = MCPWM_GEN_A, .io = MCPWM0A, .gpio = GPIO_NUM_7},
	{.unit = MCPWM_UNIT_1, .timer = MCPWM_TIMER_1, .operator = MCPWM_GEN_A, .io = MCPWM1A, .gpio = GPIO_NUM_8},
	{.unit = MCPWM_UNIT_1, .timer = MCPWM_TIMER_2, .operator = MCPWM_GEN_A, .io = MCPWM2A, .gpio = GPIO_NUM_9},
};

#define PWM_MS_MIN 1100
#define PWM_MS_MAX 1900
#define PWM_MS_IDLE 1500

void pwm_set_ms(int channel, uint16_t ms) {
	mcpwm_set_duty_in_us(pwm_map[channel].unit, pwm_map[channel].timer, pwm_map[channel].operator, ms);
}

void initPWM()
{
  ESP_LOGI(TAG, "setup MCPWM" );
  //
  for (int i = 0; i < NUM_CHANNELS; i++) {
  	mcpwm_gpio_init(pwm_map[i].unit, pwm_map[i].io, pwm_map[i].gpio );
  }
  //
  mcpwm_config_t pwm_config = {};
  pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
  pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
  pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  //
  //
  for (int i = 0; i < NUM_CHANNELS; i++) {
	mcpwm_init(pwm_map[i].unit, pwm_map[i].timer, &pwm_config);
	pwm_set_ms(PWM_MS_IDLE);
  }
}
