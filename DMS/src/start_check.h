#include "stdint.h"
#include "os.h"
#include "cx.h"

#define UNINITIALIZED 0
#define WAITING_FOR_BLOCK 1
#define WAITING_FOR_TX 2

#define ERR_WRONG_STATE_TRANSITION 0x6013
#define ERR_PREV_BLOCK_NOT_COHERENT 0x6014
#define ERR_BAD_VARINT_PARSING 0x6016

typedef struct check_ctx_t {

    uint8_t current_block_hash[32];
    uint8_t current_merkle_root[32];
    uint32_t blocks_elapsed;
    uint32_t tx_remaining_in_block;
    uint16_t tx_parsed;
    uint8_t machine_state;
    cx_sha256_t sha;
    uint8_t is_parsing_tx;
    uint8_t if_block_valid_reset_counter;
} check_ctx_t;

void init_check_ctx();

void parse_block_header(uint8_t *buffer);

unsigned long int get_varint(uint8_t *buffer);

unsigned long int btchip_read_u32(unsigned char WIDE *buffer, unsigned char be, unsigned char skipSign);

