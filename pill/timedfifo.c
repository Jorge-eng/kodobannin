#include "platform.h"
#include "app.h"

#include "timedfifo.h"
#include "util.h"

static struct{
    tf_data_t data;
    uint8_t tick;
    uint16_t current_idx;
}self;

static uint16_t _decrease_index(uint16_t * idx);

void TF_Initialize(const struct hlo_ble_time * init_time){
    memset(&self.data, 0, sizeof(self.data));
    self.data.length = sizeof(self.data);
    self.data.prev_idx = 0xFFFF;
    
    self.current_idx = 0;
    self.tick = 0;
    self.data.mtime = init_time->monotonic_time;
}


void TF_TickOneSecond(uint64_t monotonic_time){
    self.data.mtime = monotonic_time;
    if(++self.tick > MOTION_DATA_INTERVAL_SEC){
        self.tick = 0;
        self.data.prev_idx = self.current_idx;
        self.current_idx = (self.current_idx + 1) % TF_BUFFER_SIZE;
        self.data.data[self.current_idx] = 0;
        PRINTS("^");
    }else{
        PRINTS("*");
    }
}

inline tf_unit_t TF_GetCurrent(void){
    return self.data.data[self.current_idx];
}

inline void TF_SetCurrent(tf_unit_t val){
    self.data.data[self.current_idx] = val;
}

inline tf_data_t * TF_GetAll(void){
    return &self.data;
}
static
uint16_t _decrease_index(uint16_t * idx){
    if( *idx == 0){
        return TF_BUFFER_SIZE - 1;
    }else{
        return (*idx - 1);
    }
}

bool TF_GetCondensed(uint32_t* buf, uint8_t length){
    bool has_data = false;

    if(buf){
        uint16_t  i,idx = self.current_idx;
        //buf->time = self.data.mtime;

        for(i = 0; i < length; i++){
            idx = _decrease_index(&idx);
            buf[i] = self.data.data[idx];
            if(self.data.data[idx] != 0)
            {
                has_data = true;
            }
        }
    }

    return has_data;
}

inline uint8_t get_tick(){
    return self.tick;
}

