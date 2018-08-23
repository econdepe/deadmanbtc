#include "register.h"
#include "os.h"

extern N_storage_DMS_parameter;
#define N_storage (*(WIDE DMS_parameters_t *)PIC(&N_storage_DMS_parameter))


uint8_t register_param(uint8_t *buffer)
{
    /*if (N_storage.initialized != 0x01) 
    {*/
        DMS_parameters_t parameters;

        os_memcpy(parameters.pub_key, buffer, 33);
        buffer+=33;

        os_memcpy(&parameters.release_blocks_num, buffer, 4);
        buffer+=4;

        os_memcpy(parameters.first_block_hash, buffer, 32);
        buffer+=32;

        os_memcpy(parameters.secret, buffer, 64);
        buffer+=64;

        parameters.initialized = 0x01;

        nvm_write(&N_storage, (void *)&parameters, sizeof(DMS_parameters_t));
        return 0;
    /*}
    else
    {
        // Already initalized, error
        return 1;
    }*/
}