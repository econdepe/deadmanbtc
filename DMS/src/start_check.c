#include "start_check.h"
#include "register.h"
#include "os.h"

extern check_ctx_t check_ctx;

extern N_storage_DMS_parameter;
#define N_storage (*(WIDE DMS_parameters_t *)PIC(&N_storage_DMS_parameter))

void init_check_ctx()
{

    os_memset((void*)&check_ctx,0,sizeof(check_ctx));

    os_memcpy(check_ctx.current_block_hash, N_storage.first_block_hash, 32);
    check_ctx.blocks_elapsed = 0;
    check_ctx.machine_state = WAITING_FOR_BLOCK;

}


void parse_block_header(uint8_t *buffer)
{
    uint8_t *ptr = buffer+8;

    if(check_ctx.blocks_elapsed != 0)
    {
        if(os_memcmp(check_ctx.current_block_hash, ptr, 32)) THROW(ERR_PREV_BLOCK_NOT_COHERENT);
    }
    //TODO: verify difficulty and time

    ptr -= 8;


    // update block hash with current one
    cx_hash_sha256(ptr, 80, check_ctx.current_block_hash);
    cx_hash_sha256(check_ctx.current_block_hash, 32, check_ctx.current_block_hash);
    ptr += 8+32;

    // copy new merkle root
    os_memcpy(check_ctx.current_merkle_root, ptr, 32);
    // go to tx_num
    ptr = buffer+80;

    check_ctx.tx_remaining_in_block = get_varint(ptr);
    PRINTF("VARINT: %d\n", check_ctx.tx_remaining_in_block);

    check_ctx.machine_state = WAITING_FOR_TX;

}



unsigned long int get_varint(uint8_t *buffer) {
    if (buffer[0] < 0xFD) {
        return buffer[0];
    } else if (buffer[0] == 0xFD) {
        unsigned long int result;
        result =
            (unsigned long int)buffer[1] | (((unsigned long int)buffer[2]) << 8);
        return result;
    } else if (buffer[0] == 0xFE) {
        unsigned long int result;
        result =
            btchip_read_u32(buffer, 0, 0);
        return result;
    } else {
        THROW(ERR_BAD_VARINT_PARSING);
        return 0;
    }
}


unsigned long int btchip_read_u32(unsigned char WIDE *buffer, unsigned char be, unsigned char skipSign)
{
    unsigned char i;
    unsigned long int result = 0;
    unsigned char shiftValue = (be ? 24 : 0);
    for (i = 0; i < 4; i++) {
        unsigned char x = (unsigned char)buffer[i];
        if ((i == 0) && skipSign) {
            x &= 0x7f;
        }
        result += ((unsigned long int)x) << shiftValue;
        if (be) {
            shiftValue -= 8;
        } else {
            shiftValue += 8;
        }
    }
    return result;
}