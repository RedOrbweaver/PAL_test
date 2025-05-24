#include "hmain.hpp"


void dac_set_output(uint8_t value)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        gpio_put(PIN::DAC_OUT[i], value & (1 << i));
    }
}


int main()
{
    stdio_init_all();

    printf("Starting PAL_test...\n");

    for(uint8_t i = 0; i < ArraySize(PIN::DAC_OUT); i++)
    {
        init_out(PIN::DAC_OUT[i], 0);
    }

    uint8_t sin_lookup[255];

    for(int i = 0; i < 255; i++)
    {
        sin_lookup[i] = (sin(M_PI*(float(i)/255.0f)) * 255.0f);
    }

    while (true) 
    {
        for(int i = 0; i < 255; i++)
        {
            dac_set_output(sin_lookup[i]);
            //sleep_us(20);
        }
        //sleep_us(100);
    }
}
