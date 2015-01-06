#pragma once
#include "motion_types.h"

/**
 * a timed variant of a fifo in which index increases by time instead of data
 */

#include <stdint.h>
#include <stdbool.h>

#include "platform.h"
#include "app.h"
#include "hlo_ble_time.h"

/*
 * change this to be the unit of the fifo
 */

//time of each index
#define TF_UNIT_TIME_S 60 
#define TF_UNIT_TIME_MS 60000

#ifdef DATA_SCIENCE_TASK
#define TF_BUFFER_SIZE (9 * 60)
#else
#define TF_BUFFER_SIZE (2 * 60)
#endif

typedef int32_t tf_unit_t;  // Good job, this is a keen design!

typedef struct {
    uint16_t num_wakes;
    int16_t max_accel[3];
    int16_t min_accel[3];
} auxillary_data_t;

typedef struct{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t length;
    uint64_t mtime;
    uint16_t prev_idx;
    auxillary_data_t aux_data; //this came later
    tf_unit_t data[TF_BUFFER_SIZE];
}__attribute__((packed)) tf_data_t;


void TF_Initialize(const struct hlo_ble_time * init_time);
void TF_TickOneSecond(uint64_t monotonic_time);
tf_unit_t TF_GetCurrent(void);
void TF_SetCurrent(tf_unit_t val);
void TF_IncrementWakeCounts(void);
auxillary_data_t * TF_GetAuxData(void);
tf_data_t * TF_GetAll(void);
bool TF_GetCondensed(uint32_t* buf, uint8_t length);
uint8_t get_tick();
