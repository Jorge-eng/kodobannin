/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup ble_sdk_app_hrs_eval_main main.c
 * @{
 * @ingroup ble_sdk_app_hrs_eval
 * @brief Main file for Heart Rate Service Sample Application for nRF51822 evaluation board
 *
 * This file contains the source code for a sample application using the Heart Rate service
 * (and also Battery and Device Information services) for the nRF51822 evaluation board (PCA10001).
 * This application uses the @ref ble_sdk_lib_conn_params module.
 */

#include <stdint.h>
#include <string.h>

#include "platform.h"
#include "battery_config.h"
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "nrf51_bitfields.h"
#include "softdevice_handler.h"
#include "battery.h"
#include "gpio_nor.h"



/**@brief Macro to convert the result of ADC conversion in millivolts.
 *
 * @param[in]  ADC_VALUE   ADC result.
 * @retval     Result converted to millivolts.
 */

#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)   ((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS / 1023 * ADC_PRE_SCALING_COMPENSATION)
//#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)     ((((ADC_REF_VOLTAGE_IN_MILLIVOLTS)))


static batter_measure_callback_t _battery_measure_callback;
static uint8_t percentage_batt_lvl;




inline void battery_module_power_off()
{
    if(ADC_ENABLE_ENABLE_Disabled != NRF_ADC->ENABLE)
    {
        NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Disabled;
    }
#ifdef PLATFORM_HAS_VERSION
    
    gpio_cfg_s0s1_output_connect(VBAT_VER_EN, 1);
    gpio_cfg_d0s1_output_disconnect(VBAT_VER_EN);  // on: 0

    gpio_input_disconnect(VMCU_SENSE);
    gpio_input_disconnect(VBAT_SENSE);
#endif
}


static inline uint8_t _battery_level_in_percent(const uint16_t mvolts)
{
    uint8_t battery_level;

    if (mvolts >= 2883)
    {
        battery_level = 100;
    }
    else if (mvolts > 2855)
    {
        battery_level = 80;
    }
    else if (mvolts > 2828)
    {
        battery_level = 60;
    }
    else if (mvolts > 2780)
    {
        battery_level = 40;
    }
    else if (mvolts > 2750)
    {
        battery_level = 20;
    }
    else
    {
        battery_level = 5;
    }

    return battery_level;
}

/**@brief Function for handling the ADC interrupt.
 * @details  This function will fetch the conversion result from the ADC, convert the value into
 *           percentage and send it to peer.
 */
void ADC_IRQHandler(void)
{
    if (NRF_ADC->EVENTS_END != 0)
    {
        NRF_ADC->EVENTS_END     = 0;
        adc_t adc_result      = NRF_ADC->RESULT;
        NRF_ADC->TASKS_STOP     = 1;

        uint32_t battery_milvolt = adc_result;
        uint32_t batt_lvl_in_micro_volts = ADC_RESULT_IN_MILLI_VOLTS(battery_milvolt);
        percentage_batt_lvl     = _battery_level_in_percent(batt_lvl_in_micro_volts / 1000);
        

        if(_battery_measure_callback)  // I assume there is no race condition here.
        {
            _battery_measure_callback(adc_result, batt_lvl_in_micro_volts, percentage_batt_lvl);
        }
    }

    /*
    NRF_ADC->CONFIG = (ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos) | 
        (ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos) | 
        (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos) | 
        (ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos) | 
        (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
        */
    battery_module_power_off();
    
    
}
uint8_t battery_get_percent_cached(){
#ifdef PLATFORM_HAS_VERSION
    return percentage_batt_lvl;
#else
    return BATTERY_INVALID_MEASUREMENT;
#endif
}

void battery_module_power_on()
{
#ifdef PLATFORM_HAS_VERSION
    gpio_input_disconnect(VMCU_SENSE);
    gpio_cfg_s0s1_output_connect(VBAT_VER_EN, 0);
    nrf_gpio_cfg_input(VBAT_SENSE, NRF_GPIO_PIN_NOPULL);
#endif
}


uint32_t battery_measurement_begin(batter_measure_callback_t callback)
{
#ifdef PLATFORM_HAS_VERSION

    if(callback)
    {
        _battery_measure_callback = callback;
    }

    uint32_t err_code;


    // Configure ADC
    NRF_ADC->INTENSET   = ADC_INTENSET_END_Msk;
    NRF_ADC->CONFIG     = (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos)     |
                        (ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos)  |
                        (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)  |
                        (ADC_CONFIG_PSEL_AnalogInput7 << ADC_CONFIG_PSEL_Pos)    |
                        (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
  // NRF_ADC->INTENSET   = ADC_INTENSET_END_Msk;
    NRF_ADC->EVENTS_END = 0;
    NRF_ADC->ENABLE     = ADC_ENABLE_ENABLE_Enabled;

    // Enable ADC interrupt
    
    err_code = sd_nvic_ClearPendingIRQ(ADC_IRQn);
    APP_ERROR_CHECK(err_code);

    err_code = sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_LOW);
    APP_ERROR_CHECK(err_code);

    err_code = sd_nvic_EnableIRQ(ADC_IRQn);
    APP_ERROR_CHECK(err_code);
    
    NRF_ADC->EVENTS_END  = 0;    // Stop any running conversions.
    NRF_ADC->TASKS_START = 1;

    return err_code;
#else
    return NRF_SUCCESS;
#endif
}

/**
 * @}
 */
