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

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <Arduino.h>
#include <EEPROM.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
//Binding (broadcast) from esp32 seems to work only with non-os-sdk 2.2.2
#elif defined(ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#endif
#include <esp_now.h>
#include "esprx.h"
#include "uCRC16Lib.h"
#include <Ticker.h>
#include "esp_log.h"

#define TAG "RX"

int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];
int16_t fsChannelOutputs[MAX_OUTPUT_CHANNELS];

static esp_now_peer_info_t txPeer = {.peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, .channel = 2};
static esp_now_peer_info_t broadcastPeer = {
  .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
  .channel = BIND_CH,
  .ifidx = (wifi_interface_t)ESP_IF_WIFI_STA
};
static RXPacket_t packet;
static volatile bool dirty = false;
static volatile bool bindEnabled = true;
static uint32_t startTime;
static bool outputStarted = false;
uint32_t volatile packRecv = 0;
uint32_t volatile packAckn = 0;
uint32_t volatile recvTime = 0;
Ticker blinker;

char *mac2str(const uint8_t mac[6]) {
  static char buff[20];
  sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buff;
}

static bool packet_check(TXPacket_t * rxp)
{
  packet.idx++;
  uint16_t crc = rxp->crc;
  rxp->crc = 0;
  packet.crc = uCRC16Lib::calculate((char *) rxp, sizeof(TXPacket_t));
  if ( packet.crc == crc) {
    packet.idx = rxp->idx;
    packet.type = ACK;
    return true;
  }
  ESP_LOGI(TAG, "CRC16 error: %d vs %d\n", packet.crc, crc);
  return false;
}

static inline void process_data(const uint8_t mac[6], TXPacket_t *rp){
  if (packet_check(rp)) {
    recvTime = millis();
#if defined(ESP32)
    portENTER_CRITICAL(&timerMux);
#endif
    memcpy( (void*) locChannelOutputs, rp->ch, sizeof(locChannelOutputs) );
#if defined(ESP32)
    portEXIT_CRITICAL(&timerMux);
#endif
    packRecv++;
    esp_now_send(txPeer.peer_addr, (const uint8_t *) &packet, sizeof(packet)); //Send ACK
//    if (FSAFE == rp->type) {
//      dirty = true;
//    }
    if (!outputStarted && !bindEnabled) {
      initOUTPUT();
      outputStarted = true;
    }
  }
  else {
    ESP_LOGI(TAG, "Packet error from MAC: %s\n", mac2str(mac));
  }

}

static inline void process_bind(const uint8_t mac[6], TXPacket_t *rp) {
  if (BIND == rp->type) {
    uint16_t crc = rp->crc;
    rp->crc = 0;
    rp->crc = uCRC16Lib::calculate((char *) rp, sizeof(TXPacket_t));
    if(crc == rp->crc){
      ESP_LOGI(TAG, "Got bind MAC: %s", mac2str(mac));
      memcpy(txPeer.peer_addr, mac, sizeof(txPeer.peer_addr));
      memcpy( (void*) fsChannelOutputs, rp->ch, sizeof(locChannelOutputs) );
      txPeer.channel = rp->idx;
      txPeer.ifidx = (wifi_interface_t)ESP_IF_WIFI_STA;
      txPeer.encrypt = false;
      if (esp_now_is_peer_exist(txPeer.peer_addr) == false) {
        esp_now_add_peer(&txPeer);
      } else {
        esp_now_mod_peer(&txPeer);
      }
      packet.type = BIND;
      packet.crc=0;
      packet.crc = uCRC16Lib::calculate((char *) &packet, sizeof(packet));
      //esp_wifi_set_channel(BIND_CH, (wifi_second_chan_t)WIFI_SECOND_CHAN_NONE);
      esp_now_send(broadcastPeer.peer_addr, (const uint8_t *) &packet, sizeof(packet));
      dirty = true;
      bindEnabled = false;
    }
    else {
      ESP_LOGI(TAG, "Bind packet CRC error: %d vs %d", crc, rp->crc);
    }
  }
  else {
    ESP_LOGI(TAG, "Incorrect packet type: %d", rp->type);
  }
}

static void recv_cb(const uint8_t *mac, const uint8_t *buf, int count) {
  if (sizeof(TXPacket_t) == count) {
    TXPacket_t *rp = (TXPacket_t *) buf;
    if (bindEnabled && BIND == rp->type) {
      process_bind(mac, rp);
    } 
    else if (DATA == rp->type || FSAFE == rp->type) {
      if (!memcmp(mac, txPeer.peer_addr, sizeof(txPeer.peer_addr))) {
        process_data(mac, rp);
      }
      else {
        ESP_LOGI(TAG, "Wrong MAC: %s, %s\n", String(mac2str(mac)).c_str(),   String(mac2str(txPeer.peer_addr)).c_str() );
      }
    }
  }
  else {
    ESP_LOGI(TAG, "Wrong packet size: %d (must be %d)\n",  count, sizeof(TXPacket_t));
  }
}

void blink() {
  static bool fresh = true;
  uint32_t dataAge = millis()-recvTime;
  rx_led_toggle(fresh);

  if (dataAge > 100){
    if (fresh) {
      blinker.attach(1, blink);
      fresh = false;
    }
  }
  else {
    if (!fresh) {
      blinker.attach(2, blink);
      fresh = true;
    }
  }
  if (dataAge > FS_TIMEOUT) {
    memcpy(locChannelOutputs, fsChannelOutputs, sizeof(locChannelOutputs));
  }
}

void checkEEPROM() {
  static bool blinking = false;
  if (dirty) {
    disableOUTPUT();
    EEPROM.put(0,txPeer);
    EEPROM.put(sizeof(txPeer), fsChannelOutputs);
    EEPROM.commit();
    dirty = false;
    enableOUTPUT();
  }
  if (bindEnabled) {
    if(millis()-startTime > BIND_TIMEOUT) {
      bindEnabled = false;
    }
  }
  else if(!blinking) {
#if defined(ESP8266)
    wifi_set_channel(txPeer.channel);
#elif defined(ESP32)  
    esp_wifi_set_channel( txPeer.channel, (wifi_second_chan_t) 0);
#endif
    blinker.attach(0.1, blink);
    blinking = true;
  }
}
 
void initRX(){

  startTime = millis();
  rx_led_setup();

  EEPROM.begin(sizeof(txPeer)+sizeof(fsChannelOutputs));
  EEPROM.get(0,txPeer);
  EEPROM.get(sizeof(txPeer), fsChannelOutputs);

#if defined(ESP8266)
  WiFi.mode(WIFI_STA);
  wifi_set_channel(BIND_CH);  
#elif defined(ESP32)
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(BIND_CH, (wifi_second_chan_t) WIFI_SECOND_CHAN_NONE);
#endif

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_now_init());
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_now_register_recv_cb(recv_cb));

  if (ESP_OK != esp_now_add_peer(&broadcastPeer)) {
    ESP_LOGI(TAG, "ADD_PEER() failed broadcase peer\n");
  }

  txPeer.ifidx = (wifi_interface_t)ESP_IF_WIFI_STA;
  txPeer.encrypt = false;
  if (ESP_OK != esp_now_add_peer(&txPeer)) {
    ESP_LOGI(TAG, "ADD_PEER() failed MAC: %s", mac2str(txPeer.peer_addr));
  }
}

int16_t get_ch(uint16_t idx){
  return locChannelOutputs[idx];
}
