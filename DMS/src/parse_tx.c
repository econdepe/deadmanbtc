#include "parse_tx.h"
#include "start_check.h"
#include "register.h"
#include "os.h"
#include "cx.h"

extern check_ctx_t check_ctx;

extern N_storage_DMS_parameter;
#define N_storage (*(WIDE DMS_parameters_t *)PIC(&N_storage_DMS_parameter))

uint8_t parse_tx(uint8_t *buffer, uint8_t P1_more, uint8_t P2_segwit)
{


    uint8_t chunk_size = buffer[0];

    uint8_t tx_hash[32];

    // whistleblower txs always fit in one chunck
    if(!check_ctx.is_parsing_tx)
    {
        cx_sha256_init(&check_ctx.sha);

        check_ctx.is_parsing_tx = 1;
        if(P2_segwit == P2_NO_SEGWIT)
        {
            // look for pub key
            uint8_t sig_len = buffer[OFFSET_SIG_LEN];
            PRINTF("PubKey: %.*H\n", 33, buffer+OFFSET_SIG_LEN+1+sig_len+1);
            if(os_memcmp(buffer+OFFSET_SIG_LEN+1+sig_len+1, N_storage.pub_key, 33)) 
            {
                PRINTF("NOPE\n");
            }
            else
            {
                PRINTF("WHISTLEBLOWER WAS ALIVE AT BLOCK %.*H, COUNTER RESET\n", 32, check_ctx.current_block_hash);
                check_ctx.if_block_valid_reset_counter = 1;
            }
        }

        
    }
    
    
    if(P1_more == P1_LAST)
    {
        //PRINTF("TX: %.*H\n", chunk_size, buffer+1);
        cx_hash(&check_ctx.sha, CX_LAST, buffer+1, chunk_size, tx_hash);
        cx_sha256_init(&check_ctx.sha);
        cx_hash(&check_ctx.sha, CX_LAST, tx_hash, 32, tx_hash);

        //TODO: compute hmac and return it
        os_memcpy(G_io_apdu_buffer,tx_hash, 32);

        check_ctx.is_parsing_tx = 0;
        check_ctx.tx_remaining_in_block -= 1;
        return 32;
    }
    else
    {
        cx_hash(&check_ctx.sha, NULL, buffer+1, chunk_size, NULL);
        return 0;
    }

    


    
    

}


