#include "hmain.hpp"


void dac_push_output(uint32_t values)
{
    pio_sm_put(pio0, 0, values);
}

void pio_init()
{
    // pio_add_program_at_offset(pio0, &dac_out_program, 0);
    
    // for(uint8_t i = 0; i < 8; i++)
    // {
    //     pio_gpio_init(pio0, PIN::DAC_OUT[i]); 
    // }
    // auto config = dac_out_program_get_default_config(0);
    // pio_sm_set_consecutive_pindirs(pio0, 0, PIN::DAC_OUT[0], 8, true);
    // pio_sm_set_out_pins(pio0, 0, PIN::DAC_OUT[0], 8);
    // sm_config_set_out_shift(&config, true, true, 32);

    // double div = 1.0;
    // sm_config_set_clkdiv(&config, div);
    // pio_sm_init(pio0, 0, 0, &config);
    // pio_sm_set_enabled(pio0, 0, true);
    dac_program_init(pio0, 0, 0, PIN::DAC_OUT[0], 1.0f);
}


int main()
{
    stdio_init_all();

    printf("Starting PAL_test...\n");


    pio_init();

    uint8_t sin_lookup[255];

    for(int i = 0; i < 255; i++)
    {
        sin_lookup[i] = (sin(M_PI*(float(i)/255.0f)) * 255.0f);
    }

    while (true) 
    {
        for(int i = 0; i < 64; i++)
        {
            dac_push_output(*(uint32_t*)(sin_lookup+(i*4)));
            //sleep_us(100);
        }
        //sleep_us(100);
    }
}
