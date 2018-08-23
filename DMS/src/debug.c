#include "os_io_seproxyhal.h"

void debug_se_n(uint8_t *data, uint8_t len)
{
    unsigned char buff[80];
    buff[0] = 0x5F;
    buff[1] = 0;
    buff[2] = len;

    memcpy(buff+3, data, len);

    io_seproxyhal_spi_send(buff, len+3);
}

void debug_se(uint8_t data)
{
    unsigned char buff[80];
    buff[0] = 0x5F;
    buff[1] = 0;
    buff[2] = 1;
    buff[3] = data;
    io_seproxyhal_spi_send(buff, 4);
}

static unsigned int get_sp(void) {
 unsigned int sp;
 __asm volatile ("mov %0, sp":"=r"(sp));
 return sp;
}

static unsigned char tmpbuf[9];

void display_sp(void) {
    SPRINTF(tmpbuf, "%X", get_sp());
    debug_se_n(tmpbuf,8);
}