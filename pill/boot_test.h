#pragma once
#include "message_base.h"

#define ANT_OK 0x01
//#define BLE_OK 0x02
#define IMU_OK 0x04

#define ALL_OK (ANT_OK | IMU_OK)

void test_ok(const MSG_Central_t * parent, uint8_t mask);
