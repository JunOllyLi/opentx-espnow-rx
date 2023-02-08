#ifndef ESPRX_H
#define ESPRX_H
#include "esprc_packet.h"

#define GPIO_PPM_PIN 5
#define BIND_TIMEOUT 1000
#define FS_TIMEOUT 1000

const uint8_t broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
extern volatile uint32_t packRecv;
extern volatile uint32_t packAckn;
extern volatile uint32_t recvPeriod;
extern int16_t locChannelOutputs[MAX_OUTPUT_CHANNELS];
extern int16_t fsChannelOutputs[MAX_OUTPUT_CHANNELS];
#if defined(ESP32)
extern portMUX_TYPE timerMux;
#endif

void initRX();
void checkEEPROM();

extern int16_t locChannelOutputs[];
extern int16_t fsChannelOutputs[];

void initOUTPUT();
void disableOUTPUT();
void enableOUTPUT();

void rx_led_setup();
void rx_led_toggle(bool data_fresh);
#endif
