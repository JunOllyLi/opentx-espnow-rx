/* 
 * This file is part of the opentx-espnow-rx(https://github.com/tmertelj/opentx-espnow-rx).
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
#include "Arduino.h"
#include "esprx.h"
#include "esp_log.h"

int16_t get_ch(uint16_t idx);
extern uint32_t volatile recvTime;

#define TAG "ESPNOW_RX"

void setup() {
  initRX();
}

void loop()
{
  static char buff[8*10];
  delay(1000);
  checkEEPROM();
  *buff = 0;
  char *p = buff;
  for(int i = 0; i < 8; i++){
    sprintf(p,"%2d:%4d, ", i, get_ch(i));
    p = buff + strlen(buff);
  }
  ESP_LOGI(TAG, "Data age (ms): %lu received: %u", millis()- recvTime, packRecv);
  ESP_LOGI(TAG, "%s", buff);
}
