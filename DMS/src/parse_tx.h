#include "stdint.h"

#define ERR_WRONG_TX_FLAGS 0x6015

#define P1_LAST 0x80
#define P1_MORE 0x00
#define P2_SEGWIT 0x80
#define P2_NO_SEGWIT 0x00

#define OFFSET_SIG_LEN 43

uint8_t parse_tx(uint8_t *buffer, uint8_t P1_more, uint8_t P2_segwit);
