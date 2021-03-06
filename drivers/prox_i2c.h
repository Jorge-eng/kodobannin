#pragma once
#include "message_base.h"

MSG_Status init_prox(void);
void read_prox(uint32_t * out_val1, uint32_t * out_val4);
void set_prox_offset(int16_t off1, int16_t off4);
void set_prox_gain(uint16_t gain1, uint16_t gain4);
