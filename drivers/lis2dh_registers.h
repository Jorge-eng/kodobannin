#pragma once

// vi:noet:sw=4 ts=4

// Based on LIS2DH datasheet

#define CHIP_ID 0x33

typedef enum IMU_Registers {
	REG_STATUS_AUX = 0x7,
	REG_OUT_TEMP_LO = 0xc,
	REG_OUT_TEMP_HI = 0xd,
	REG_INT_COUNTER = 0xe,
	REG_WHO_AM_I = 0xf,
	REG_TEMP_CFG = 0x1f,
	REG_CTRL_1 = 0x20,
	REG_CTRL_2 = 0x21,
	REG_CTRL_3 = 0x22,
	REG_CTRL_4 = 0x23,
	REG_CTRL_5 = 0x24,
	REG_CTRL_6 = 0x25,
	REG_REFERENCE = 0x26,
	REG_STATUS_2 = 0x27,
	REG_ACC_X_HI = 0x28,
	REG_ACC_X_LO = 0x29,
	REG_ACC_Y_HI = 0x2a,
	REG_ACC_Y_LO = 0x2b,
	REG_ACC_Z_HI = 0x2c,
	REG_ACC_Z_LO = 0x2d,
	REG_FIFO_CTRL = 0x2e,
	REG_FIFO_SRC = 0x2f,
	REG_INT1_CFG = 0x30,
	REG_INT1_SRC = 0x31,
	REG_INT1_THR = 0x32,
	REG_INT1_DUR = 0x33,
	REG_INT2_CFG = 0x34,
	REG_INT2_SRC = 0x35,
	REG_INT2_THR = 0x36,
	REG_INT2_DUR = 0x37,
	REG_CLICK_CFG = 0x38,
	REG_CLICK_SRC = 0x39,
	REG_CLICK_THR = 0x3a,
	REG_TIME_LIMIT = 0x3b,
	REG_TIME_LATENCY = 0x3c,
	REG_TIME_WINDOW = 0x3d,
	REG_ACT_THR = 0x3e,
	REG_ACT_DUR = 0x3f,

} Register_t;

#define ACCEL_CFG_SCALE_OFFSET 4

enum IMU_Reg_Bits {
	//REG_TEMP_CFG
	TEMPERATURE_DATA_OVERRUN  = 0x40,
	TEMPERATURE_DATA_AVAILABLE  = 0x4,
	TEMPERATURE_ENABLE = 0xc0,

	//REG_CTRL_1
	OUTPUT_DATA_RATE = 0xf0,
	LOW_POWER_MODE = 0x8,
	AXIS_ENABLE = 0x7,

	//REG_CTRL_2
	HIGHPASS_FILTER_MODE = 0xc0,
	HIGHPASS_FILTER_CUTOFF = 0x30,
	FILTERED_DATA_SELECTION = 0x7,
	HIGHPASS_FILTER_CLICK = 0x4,
	HIGHPASS_AOI_INT2 = 0x2,
	HIGHPASS_AOI_INT1 = 0x1,

	//REG_CTRL_3
	INT1_CLICK = 0x80,
	INT1_AOI1 = 0x40,
	INT1_AOI2 = 0x20,
	INT1_DRDY1 = 0x10,
	INT1_DRDY2 = 0x8,
	INT1_FIFO_WATERMARK = 0x4,
	INT1_FIFO_OVERRUN = 0x2,

	//REG_CTRL_4
	BLOCKDATA_UPDATE = 0x80, //force atomic lsb and msb reads
	ENDIANNESS_SELECTION = 0x40,
	FS_MASK = 0x30,
	HIGHRES = 0x08,
	SELFTEST_ENABLE = 0x06,
	SPI_SERIAL_MODE = 0x01, //0 == 4 wire, 1 == 3 wire

	//REG_CTRL_5
	BOOT = 0x80,
	FIFO_EN = 0x40,
	LATCH_INTERRUPT1 = 0x8, // INT1_SRC cleared by reading INT1_SRC only
	LATCH_INTERRUPT2 = 0x2,

	//REG_CTRL_6
	INT2_CLICK_ENABLE = 0x80,
	INT1_OUTPUT_ON_LINE_2 = 0x40, //output interrupt generator 2 on interrupt line 1...
	INT2_OUTPUT_ON_LINE_2 = 0x20,
	BOOT_INT2 = 0x8, //boot on assertion of interrupt 2
	INT_ACTIVE = 0x2, // 1 = active high, 0 = active low

	//REG_FIFO_CTRL
	FIFO_MODE_SELECTION = 0xc0,
	FIFO_TRIGGER_SELECTION = 0x40,
	FIFO_WATERMARK_THRESHOLD = 0x1f,

	//REG_FIFO_SRC
	FIFO_WATERMARK = 0x80,
	FIFO_OVERRUN = 0x40,

	//REG_INT1_CFG
	INT1_AND_OR = 0x80,
	INT1_6D = 0x40,
	INT1_Z_HIGH = 0x20,
	INT1_Z_LOW = 0x10,
	INT1_Y_HIGH = 0x8,
	INT1_Y_LOW = 0x4,
	INT1_X_HIGH = 0x2,
	INT1_X_LOW = 0x1,

	//INT1_SRC
	//... we will just look for not zero...
	// reading this one will clear the interrupt in latched mode

	//INT1_THS
	//just a number... 1lsb = 16mg at 2g

	//INT1_DURATION
	//minimum duration to trigger interrupt 1...


	//INT2, click won't be used.
};
