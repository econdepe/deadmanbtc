#include <stdint.h>

#define ERR_ALREADY_REGISTERED 0x6011
#define ERR_NVRAM_NOT_INITIALIZED 0x6012

typedef struct DMS_parameters_t {

    uint8_t initialized;
    uint8_t pub_key[33];
    uint32_t release_blocks_num;
    uint8_t first_block_hash[32];
    uint8_t secret[64];
} DMS_parameters_t;


uint8_t register_param(uint8_t *buffer);