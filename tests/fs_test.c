// vi:noet:sw=4 ts=4

//xcrun clang ../drivers/hlo_fs.c -I. -I.. -DTEST_HARNESS -DDEBUG_SERIAL -c && xcrun clang fs_test.c -I.. -DTEST_HARNESS hlo_fs.o

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <drivers/spi_nor.h>
#include <drivers/hlo_fs.h>

#define BLOCK_SIZE 4096

static FILE *data_file = NULL;

// we simulate a 32MiB Macronix chip
static NOR_Chip_Config _nor_cfg = {
	.vendor_id = NOR_Mfg_Macronix,
	.chip_id   = NOR_Chip_MX25U256,
	.capacity  = 32 * 1024 * 1024,
	.block_size = 4096,
	.page_size  = 256,
	.total_blocks = 8192,
	.pages_per_block = 16,
};

NOR_Chip_Config *
spinor_get_chip_config() {
	return &_nor_cfg;
}

int32_t
spinor_in_secure_mode() {
	return 0;
}

int32_t
spinor_exit_secure_mode() {
	return 0;
}

int32_t
spinor_read(uint32_t address, uint32_t len, uint8_t *buffer) {
	fseek(data_file, address, SEEK_SET);
	return fread(buffer, 1, len, data_file);
}

int32_t
spinor_write(uint32_t address, uint32_t len, uint8_t *buffer) {
	fseek(data_file, address, SEEK_SET);
	return fwrite(buffer, 1, len, data_file);
}

int32_t
spinor_write_page(uint32_t address, uint32_t len, uint8_t *buffer) {
	// this might not be right, check the SPI command syntax
	return spinor_write(address, len, buffer);
}

int32_t
spinor_block_erase(uint16_t block) {
	uint8_t data[BLOCK_SIZE];

	memset(data, 0xFF, BLOCK_SIZE);

	return BLOCK_SIZE == spinor_write(block * BLOCK_SIZE, BLOCK_SIZE, data) ? 0 : -1;
}

int32_t
spinor_block_erase_64(uint16_t block) {
	int i;
	int32_t ret;
	uint8_t data[BLOCK_SIZE];

	memset(data, 0xFF, BLOCK_SIZE);

	for (i = 0; i < 16; i++) {
		ret = spinor_write(((block * 16) + i) * BLOCK_SIZE, BLOCK_SIZE, data);
		if (ret != BLOCK_SIZE)
			return -1;
	}

	return 0;
}

int32_t
spinor_chip_erase() {
	int i;
	uint8_t data[BLOCK_SIZE];
	int32_t ret;

	memset(data, 0xFF, BLOCK_SIZE);

	for(i=0; i < _nor_cfg.total_blocks; i++) {
		ret = spinor_write(i * BLOCK_SIZE, BLOCK_SIZE, data);
		if (ret != BLOCK_SIZE) {
			printf("%s: short write of %d bytes for block %d of %d\n", __FUNCTION__, ret, i, _nor_cfg.total_blocks);
			return -1;
		}
	}
	return 0;
}

void
spinor_wait_completion() {
}

int32_t
fstest_init(const char *filename) {
	if (data_file) {
		fclose(data_file);
	}
	data_file = fopen(filename, "rb+");
	if (!data_file)
		return -1;

	return 0;
}


#define STRIDE 32
void
print_page(uint8_t *ptr, uint32_t len)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < len; i+=STRIDE) {
		for (j=0; j < STRIDE; j++) {
			if (i+j >= len) {
				break;
			}
			printf("%02X ", ptr[i+j]);
		}
        printf("\r\n");
    }
}

int
main(int argc, const char *argv[]) {
	int i;
	int32_t ret;
	HLO_FS_Partition_Info parts[2];
	uint8_t buffer[256];
	uint8_t read_buffer[576];

	for (i=1; i < argc; i++) {
		printf("\nTesting file '%s'\n", argv[i]);
		ret = fstest_init(argv[i]);
		if (ret != 0)
			printf("\tfailed\n");
		ret = hlo_fs_init();
		printf("\tfs_init ret %d\n", ret);
		if (ret == HLO_FS_Not_Initialized) {
	        parts[0].id = HLO_FS_Partition_Crashlog;
    	    parts[0].block_offset = -1;
       		parts[0].block_count = 5; // 20k
 			ret = hlo_fs_format(1, parts, 1);
			printf("\tfs_format ret 0x%X\n", ret);
        	ret = hlo_fs_init();
        	printf("\tfs_init ret 0x%X\n", ret);

    	}
		for(int j = 0; j < 32; j++) {
			memset(buffer, j, 256);
			ret = hlo_fs_append(HLO_FS_Partition_Crashlog, 256, buffer);
			if (ret < 0) {
				printf("append failed with 0x%x for iteration %i\n", ret, j);
				goto end;
			} else {
				printf("\tappend ret 0x%X\n", ret);
			}
		}

		HLO_FS_Page_Range range;
		ret = hlo_fs_read(HLO_FS_Partition_Crashlog, 256, read_buffer, &range);
		if (ret < 0) {
			printf("reading returned error 0x%x\n\n", ret);
			goto end;
		}

		printf("partition read returned %d bytes:\n", ret);
		print_page(read_buffer, ret);
		//invalidate range
		ret = hlo_fs_mark_dirty(HLO_FS_Partition_Crashlog, &range);
		printf("mark dirty returned %d\n", ret);

		range.token = 0;
		ret = hlo_fs_read(HLO_FS_Partition_Crashlog, 256, read_buffer, &range);
		if (ret > 0) {
			printf("partition read returned %d bytes:\n", ret);
			print_page(read_buffer, ret);
		}
/*
		do {
			ret = hlo_fs_read(HLO_FS_Partition_Crashlog, 256, read_buffer, &range);
			if (ret > 0) {
				printf("\tpartition read returned %d bytes:\n", ret);
				print_page(read_buffer, ret);
			}

		} while (ret > 0);
*/
end: {}
	}

	if (data_file)
		fclose(data_file);
}
