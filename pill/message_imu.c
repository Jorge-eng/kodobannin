// vi:noet:sw=4 ts=4
#include "app.h"
#include "platform.h"

#include <app_timer.h>
#include <spi.h>
#include <simple_uart.h>
#include <util.h>

#include <app_error.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <string.h>
#include <app_gpiote.h>
#include "imu.h"

#include "message_imu.h"
#include "mpu_6500_registers.h"
#include "sensor_data.h"
#include "message_base.h"
#include "timedfifo.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include "message_ant.h"
#endif

#include <watchdog.h>
#include "shake_detect.h"
#include "gpio_nor.h"

#include "message_time.h"
#include "pill_gatt.h"
#include "message_prox.h"
#include "hble.h"

enum {
    IMU_COLLECTION_INTERVAL = 6553, // in timer ticks, so 200ms (0.2*32768)
};

static app_timer_id_t _wom_timer;
static app_gpiote_user_id_t _gpiote_user;
static uint32_t _last_active_time;

static char * name = "IMU";
static bool initialized = false;

static const MSG_Central_t * parent;
static MSG_Base_t base;
static uint8_t stuck_counter;
static uint32_t top_of_minute = 0;
static uint32_t active_time = 0;

static void _update_motion_mask(uint32_t now, uint32_t anchor);

static struct imu_settings _settings = {
	.active_wom_threshold = IMU_ACTIVE_WOM,
    .inactive_wom_threshold = IMU_INACTIVE_WOM,
	.inactive_sampling_rate = IMU_INACTIVE_FREQ,  //IMU_HZ_15_63; IMU_HZ_31_25; IMU_HZ_62_50; IMU_HZ_7_81; IMU_HZ_3_91; IMU_HZ_1_95; IMU_HZ_0_98
#ifdef IMU_DYNAMIC_SAMPLING
    .active_sampling_rate = IMU_ACTIVE_FREQ, //IMU_HZ_15_63; IMU_HZ_31_25; IMU_HZ_62_50; IMU_HZ_7_81; IMU_HZ_3_91; IMU_HZ_1_95; IMU_HZ_0_98
#else
    .active_sampling_rate = IMU_CONSTANT_FREQ,
#endif
	.active_sensors = IMU_SENSORS_ACCEL,//|IMU_SENSORS_GYRO,
    .accel_range = IMU_ACCEL_RANGE_2G,
    .data_ready_callback = NULL,
    .is_active = false,
};


static inline void _reset_accel_range(enum imu_accel_range range)
{
	_settings.accel_range = range;
    imu_set_accel_range(range);
}


inline void imu_get_settings(struct imu_settings *settings)
{
	*settings = _settings;
}



static void _dispatch_motion_data_via_ant(const int16_t* values, size_t len)
{
#ifdef ANT_STACK_SUPPORT_REQD
	/* do not advertise if has at least one bond */
	MSG_Data_t * message_data = MSG_Base_AllocateDataAtomic(len);
	if(message_data){
		memcpy(message_data->buf, values, len);
		parent->dispatch((MSG_Address_t){0, 0},(MSG_Address_t){ANT, 1}, message_data);
		MSG_Base_ReleaseDataAtomic(message_data);
	}
#endif
}

static uint32_t _aggregate_motion_data(const int16_t* raw_xyz, size_t len)
{
    int16_t values[3];
//    uint16_t range;
//    uint16_t maxrange
    //auxillary_data_t * paux;
    memcpy(values, raw_xyz, len);

    //int32_t aggregate = ABS(values[0]) + ABS(values[1]) + ABS(values[2]);
    uint32_t aggregate = values[0] * values[0] + values[1] * values[1] + values[2] * values[2];
	
	tf_unit_t* current = TF_GetCurrent();
    ++current->num_meas;
    if(current->max_amp < aggregate){
        current->max_amp = aggregate;
        PRINTF( "NEW MAX: %u\r\n", aggregate);
    }
    for(int i=0;i<3;++i){
        current->avg_accel[i] += (values[i] - current->avg_accel[i])/current->num_meas;
    }
    return aggregate;
}

static void _imu_switch_mode(bool is_active)
{
    if(is_active)
    {
#ifdef IMU_ENABLE_LOW_POWER
        imu_enter_normal_mode();
#endif
        imu_set_accel_freq(_settings.active_sampling_rate);
//        imu_wom_set_threshold(_settings.active_wom_threshold); todo this is not meaninful anymore
      
        app_timer_start(_wom_timer, IMU_ACTIVE_INTERVAL, NULL);

        PRINTS("IMU Active.\r\n");
        _settings.is_active = true;
    }else{
#ifdef IMU_ENABLE_LOW_POWER
        imu_enter_low_power_mode();
#endif
        ShakeDetectReset(SHAKING_MOTION_THRESHOLD);
        imu_set_accel_freq(_settings.inactive_sampling_rate);
//        imu_wom_set_threshold(_settings.inactive_wom_threshold); //only set this once in init

        app_timer_stop(_wom_timer);
        PRINTS("IMU Inactive.\r\n");
        _settings.is_active = false;
    }
}

static bool reading = false;
static void _imu_gpiote_process(uint32_t event_pins_low_to_high, uint32_t event_pins_high_to_low)
{
    APP_OK(app_gpiote_user_disable(_gpiote_user));
    if( !reading ) {
        parent->dispatch( (MSG_Address_t){IMU, 0}, (MSG_Address_t){IMU, IMU_READ_XYZ}, NULL);
        reading = true;
    }
    PRINTS("I\r\n");
}

#define PRINT_HEX_X(x) PRINT_HEX(&x, sizeof(x)); PRINTS("\r\n");

static void _update_motion_mask(uint32_t now, uint32_t anchor){
    uint32_t time_diff = 0;
    app_timer_cnt_diff_compute(now, anchor, &time_diff);
    time_diff /= APP_TIMER_TICKS( 1000, APP_TIMER_PRESCALER );

	uint64_t old_mask = TF_GetCurrent()->motion_mask;
    TF_GetCurrent()->motion_mask |= 1ull<<(time_diff%60);

	if(TF_GetCurrent()->motion_mask != old_mask){
		PRINTS("mask\r\n");
		PRINT_HEX_X( TF_GetCurrent()->motion_mask  );
	}
}

static void _on_wom_timer(void* context)
{
    uint32_t current_time = 0;
    app_timer_cnt_get(&current_time);

    uint32_t active_time_diff = 0;
    app_timer_cnt_diff_compute(current_time, active_time, &active_time_diff);

    ShakeDetectDecWindow();

    if(active_time_diff >= IMU_SLEEP_TIMEOUT && _settings.is_active)
    {
        _imu_switch_mode(false);
    }
}

uint8_t
clear_stuck_count(void)
{
	uint8_t value = stuck_counter;
	stuck_counter = 0;
	return value;
}

void top_of_meas_minute(void) {
    app_timer_cnt_get(&top_of_minute);
}

uint8_t
fix_imu_interrupt(void){
	uint32_t gpio_pin_state;
	uint8_t value = 0;
	if(initialized){
		if(NRF_SUCCESS == app_gpiote_pins_state_get(_gpiote_user, &gpio_pin_state)){
			if(!(gpio_pin_state & (1<<IMU_INT))){
				parent->dispatch( (MSG_Address_t){IMU, 0}, (MSG_Address_t){IMU, IMU_READ_XYZ}, NULL);
				if (stuck_counter < 15)
				{
					++stuck_counter;
				}
				value = stuck_counter; // talley imu int stuck low
			}else{
			}
		}else{
		}
	}
	return value;
}
static void _send_shake(void){
	static uint8_t counter;
#ifdef ANT_ENABLE
    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANT_PillData_t) + sizeof(pill_shakedata_t));
    if(data_page){
        memset(&data_page->buf, 0, sizeof(data_page->len));
        MSG_ANT_PillData_t* ant_data = (MSG_ANT_PillData_t*)&data_page->buf;
		pill_shakedata_t * shake_data = (pill_shakedata_t*)ant_data->payload;
        ant_data->version = ANT_PROTOCOL_VER;
        ant_data->type = ANT_PILL_SHAKING;
        ant_data->UUID = GET_UUID_64();
		shake_data->counter = counter++;
        parent->dispatch((MSG_Address_t){IMU,1}, (MSG_Address_t){ANT,1}, data_page);
        MSG_Base_ReleaseDataAtomic(data_page);
    }
#endif
}

static void _on_pill_pairing_guesture_detected(void){
    //TODO: send pairing request packets via ANT
	_send_shake();
#ifdef BLE_ENABLE
    hble_advertising_start();
#endif

    PRINTS("Shake detected\r\n");
}


static MSG_Status _init(void){
	if(!initialized){
		nrf_gpio_cfg_input(IMU_INT, NRF_GPIO_PIN_NOPULL);

#ifdef IMU_DYNAMIC_SAMPLING
		if(!imu_init_low_power(SPI_Channel_1, SPI_Mode3, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS, 
			_settings.inactive_sampling_rate, _settings.accel_range, _settings.inactive_wom_threshold))
#else
        if(!imu_init_low_power(SPI_Channel_1, SPI_Mode3, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS, 
            _settings.active_sampling_rate, _settings.accel_range, _settings.active_wom_threshold))
#endif
		{
		    imu_clear_interrupt_status();
			APP_OK(app_gpiote_user_enable(_gpiote_user));
			PRINTS("IMU: initialization done.\r\n");
			initialized = true;
        }
        app_timer_cnt_get(&top_of_minute);

        imu_wom_set_threshold(_settings.inactive_wom_threshold);
	}
    return SUCCESS;
}

static MSG_Status _destroy(void){
	if(initialized){
		initialized = false;
		APP_OK(app_gpiote_user_disable(_gpiote_user));
		imu_clear_interrupt_status();
		gpio_input_disconnect(IMU_INT);
		imu_power_off();
	}
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}
static MSG_Status _handle_self_test(void){
	MSG_Status ret = FAIL;
	if( !imu_self_test() ){
		ret = SUCCESS;
	}
	parent->unloadmod(&base);
	parent->loadmod(&base);
	return ret;
}


static MSG_Status _handle_read_xyz(void){

#ifdef IMU_FIFO_ENABLE
	{
	int16_t values[IMU_FIFO_CAPACITY_WORDS]; //todo check if the stack can handle this
	uint8_t ret;
	int16_t* ptr = values;
	uint32_t mag;

	// Returns number of bytes read, 0 if no data read
	ret = imu_handle_fifo_read(values);

	if(ret){

		// FIFO read, handle values

		uint8_t i;
		//loop:
		for(i=0;i<ret/6;i++)
		{
			//aggregate value greater than threshold
			mag = _aggregate_motion_data(ptr, 3*sizeof(int16_t));
			ShakeDetect(mag);

			ptr += 3;
		}

	}
	}

#else
	int16_t values[3];
	uint32_t mag;

	imu_accel_reg_read((uint8_t*)values);
    PRINTS("R\r\n");

	mag = _aggregate_motion_data(values, sizeof(values));
	ShakeDetect(mag);
#endif

    reading = false;

#ifdef IMU_DYNAMIC_SAMPLING
	if(!_settings.is_active)
	{
		_imu_switch_mode(true);
		app_timer_cnt_get(&_last_active_time);
	    app_timer_cnt_get(&active_time);
	}
#endif

    APP_OK(app_gpiote_user_enable(_gpiote_user));


	uint8_t interrupt_status = imu_clear_interrupt_status();
	if( interrupt_status ) {
	    uint32_t current_time = 0;
	    app_timer_cnt_get(&current_time);

	    _update_motion_mask(current_time, top_of_minute);
	}

	return SUCCESS;
}

static MSG_Status _send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
	MSG_Status ret = SUCCESS;
	switch(dst.submodule){
		default:
		case IMU_PING:
			PRINTS(name);
			PRINTS("\r\n");
			break;
		case IMU_READ_XYZ:
			ret = _handle_read_xyz();
			break;
		case IMU_SELF_TEST:
			ret = _handle_self_test();
			break;
		case IMU_FORCE_SHAKE:
			_on_pill_pairing_guesture_detected();
			break;
	}
	return ret;
}


MSG_Base_t * MSG_IMU_Init(const MSG_Central_t * central)
{
	imu_power_off();

	parent = central;
	base.init = _init;
	base.destroy = _destroy;
	base.flush = _flush;
	base.send = _send;
	base.type = IMU;
	base.typestr = name;
#ifdef IMU_DYNAMIC_SAMPLING
	APP_OK(app_timer_create(&_wom_timer, APP_TIMER_MODE_REPEATED, _on_wom_timer));
#endif
	APP_OK(app_gpiote_user_register(&_gpiote_user, 0, 1 << IMU_INT, _imu_gpiote_process));
	APP_OK(app_gpiote_user_disable(_gpiote_user));
    ShakeDetectReset(SHAKING_MOTION_THRESHOLD);
    set_shake_detection_callback(_on_pill_pairing_guesture_detected);

	return &base;

}
MSG_Base_t * MSG_IMU_GetBase(void){
	return &base;
}
