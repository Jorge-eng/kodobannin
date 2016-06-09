#include "message_prox.h"
#include "platform.h"
#include "util.h"
#include "prox_i2c.h"
#include "twi_master.h"
#include "message_ant.h"
#include "app_timer.h"
#include "twi_master_config.h"

static char * name = "PROX";
static const MSG_Central_t * parent;
static MSG_Base_t base;
static app_timer_id_t timer_id_prox;

#define PROX_POLL_INTERVAL 15000 /*in ms*/
#define MSB_24_MASK (1<<23)
#define VALID_PROX_RANGE(x) (x &&  x <= (1<<21))

typedef struct {
    uint16_t gain[2];
    int16_t  offset[2];
}prox_calibration_t;

static int16_t get_offset(uint32_t meas){
    int inv = meas;
    if(meas & MSB_24_MASK){//negative
        inv = (1<<23) - (meas & 0x007FFFFF);
    }else{//positive
        inv = inv * -1;
    }
    return (inv/(1<<19) + 1);
}
static uint16_t get_gain(uint32_t meas){
    return 0xFFFF;
}
static int _get_calibration(prox_calibration_t * out){
    return 1;//doesn't work atm
}
static void _notify_calibration_result(uint8_t good){
    if(good){
        hlo_ble_notify(0xD00D, "Pass", 4, NULL);
        PRINTF("Calibration Pass\r\n");
    }else{
        hlo_ble_notify(0xD00D, "Fail", 4, NULL);
        PRINTF("Calibration Fail\r\n");
    }

}
static void _do_prox_calibration(void){
    uint32_t cap1 = 0;
    uint32_t cap4 = 0;
    PRINTF("Prox Calibration\r\n");
    read_prox(&cap1, &cap4);
    PRINTF("Got [%u, %u]\r\n", cap1, cap4);
    if(VALID_PROX_RANGE(cap1) && VALID_PROX_RANGE(cap4)){
        //don't need to recalibrate
        PRINTF("Skipping Calibration [%u, %u]\r\n", cap1, cap4);
        _notify_calibration_result(1);
        return;
    }

    set_prox_offset(get_offset(cap1), get_offset(cap4));
    set_prox_gain(get_gain(cap1), get_gain(cap4));

    read_prox(&cap1, &cap4);
    PRINTF("Got [%u, %u]\r\n", cap1, cap4);
    //check if values are reasonable, if so, commit
    if(VALID_PROX_RANGE(cap1) && VALID_PROX_RANGE(cap4)){
        //ok
        //commit
        _notify_calibration_result(1);
    }else{
        //doesn't work, reset
        _notify_calibration_result(0);
    }

}
static MSG_Status _init(void){
    uint32_t ticks = 0;
    ticks = APP_TIMER_TICKS(PROX_POLL_INTERVAL,APP_TIMER_PRESCALER);
    PRINTF("PROX %u ms\r\n", PROX_POLL_INTERVAL);
    app_timer_start(timer_id_prox, ticks, NULL);
    {
        //load calibration values
        prox_calibration_t c;
        if(0 == _get_calibration(&c)){
            set_prox_offset(c.offset[0], c.offset[1]);
            set_prox_gain(c.gain[0], c.gain[1]);
        }
        /*
         *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){PROX,PROX_CALIBRATE}, NULL);
         */
    }

    return init_prox();
}
static MSG_Status _destroy(void){
    return SUCCESS;
}
static MSG_Status _flush(void){
    return SUCCESS;
}
static MSG_Status _on_message(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    PRINTS("PROX CMD\r\n");
    switch(dst.submodule){
        default:
        case PROX_PING:
            break;
        case PROX_READ:
            break;
        case PROX_CALIBRATE:
            _do_prox_calibration();
            break;
    }
    return SUCCESS;
}
static void _send_available_prox_ant(){
    pill_proxdata_t prox = (pill_proxdata_t){
        .cap = {0},
        .reserved = 0,
    };
    read_prox(&prox.cap[0], &prox.cap[1]);
    PRINTF("P1: %u, P4: %u \r\n", prox.cap[0], prox.cap[1]);
    {
    MSG_Data_t * data = AllocateEncryptedAntPayload(ANT_PILL_PROX_ENCRYPTED, &prox, sizeof(prox));
    if(data){
        parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){ANT,1}, data);
        /*
         *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){UART,MSG_UART_HEX}, data);
         */
        MSG_Base_ReleaseDataAtomic(data);
    }
    }
    {
    MSG_Data_t * data = AllocateAntPayload(ANT_PILL_PROX_PLAINTEXT, &prox, sizeof(prox));
    if(data){
        parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){ANT,1}, data);
        /*
         *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){UART,MSG_UART_HEX}, data);
         */
        MSG_Base_ReleaseDataAtomic(data);
    }
    }
}
static void _prox_timer_handler(void * ctx) {
    _send_available_prox_ant();
}
MSG_Base_t * MSG_Prox_Init(const MSG_Central_t * central){
	parent = central;
	base.init = _init;
	base.destroy = _destroy;
	base.flush = _flush;
	base.send = _on_message;
	base.type = PROX;
	base.typestr = name;

    twi_master_init();
    APP_OK(app_timer_create(&timer_id_prox,APP_TIMER_MODE_REPEATED, _prox_timer_handler));
    return &base;
}
