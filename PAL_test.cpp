#include "hmain.hpp"


void dac_push_output(uint32_t values)
{
    pio_sm_put(pio0, 0, values);
}


void dma_init(pio_hw_t* pio, int sm, uint8_t* source, uint32_t len)
{
    assert(len % 4 == 0);

    static uint8_t* lookup_source = source;

    auto dmaDataChan = dma_claim_unused_channel(true);
    auto dmaCtrlChan = dma_claim_unused_channel(true);

    // Configure the control channel to restart the data channel when it's done
    auto ctrlChanConfig = dma_channel_get_default_config(dmaCtrlChan);
    channel_config_set_transfer_data_size(&ctrlChanConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&ctrlChanConfig, false);
    channel_config_set_write_increment(&ctrlChanConfig, false);
    channel_config_set_chain_to(&ctrlChanConfig, dmaDataChan);
    channel_config_set_irq_quiet(&ctrlChanConfig, true);
    channel_config_set_high_priority(&ctrlChanConfig, true);
    channel_config_set_enable(&ctrlChanConfig, true);
    dma_channel_configure(
        dmaCtrlChan, 
        &ctrlChanConfig, 
        // Write to the read address of the data channel
        &dma_hw->ch[dmaDataChan].read_addr,
        // Read from the sample points data pointer
        &lookup_source,
        // One 32-bit word
        1,
        // Don't start yet
        false);

    // Configure the data channel to output 32 bits at a time to the PIO state machine
    auto dataChanConfig = dma_channel_get_default_config(dmaDataChan);
    channel_config_set_transfer_data_size(&dataChanConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dataChanConfig, true);
    channel_config_set_write_increment(&dataChanConfig, false);
    channel_config_set_dreq(&dataChanConfig, pio_get_dreq(pio, sm, true));
    channel_config_set_chain_to(&dataChanConfig, dmaCtrlChan);
    channel_config_set_irq_quiet(&dataChanConfig, true);
    channel_config_set_high_priority(&dataChanConfig, true);
    channel_config_set_enable(&dataChanConfig, true);
    dma_channel_configure(
        dmaDataChan,
        &dataChanConfig,
        // Write to the FIFO
        &pio->txf[sm],
        // Read from the sample points data
        lookup_source,
        len / 4,
        // Don't start yet
        false);

    dma_start_channel_mask((1u << dmaDataChan) | (1u << dmaCtrlChan));

    //while(true) tight_loop_contents();

    // auto dataChanConfig = dma_channel_get_default_config(dmaDataChan);
    // channel_config_set_transfer_data_size(&dataChanConfig, DMA_SIZE_32);
    // channel_config_set_read_increment(&dataChanConfig, true);
    // channel_config_set_write_increment(&dataChanConfig, false);
    // channel_config_set_dreq(&dataChanConfig, pio_get_dreq(pio, sm, true));
    // //channel_config_set_chain_to(&dataChanConfig, dmaCtrlChan);
    // //channel_config_set_irq_quiet(&dataChanConfig, true);
    // channel_config_set_high_priority(&dataChanConfig, true);
    // channel_config_set_enable(&dataChanConfig, true);
    // dma_channel_configure(
    //     dmaDataChan,
    //     &dataChanConfig,
    //     // Write to the FIFO
    //     &pio->txf[sm],
    //     // Read from the sample points data
    //     source,
    //     len / 4,
    //     // Don't start yet
    //     false);
    
    // dma_channel_start(dmaDataChan);
    // while(true)
    // {
    //     dma_channel_set_read_addr(dmaDataChan, source, true);
    //     dma_channel_wait_for_finish_blocking(dmaDataChan);
    // }
}

void pio_init(uint8_t* source, uint len)
{
    auto pio = pio0;
    auto sm = pio_claim_unused_sm(pio0, true);

    uint8_t offset = pio_add_program(pio, &dac_out_program);
    dac_program_init(pio, sm, offset, PIN::DAC_OUT[0], 1.0f);
    dma_init(pio, sm, source, len);
}

int main()
{
    stdio_init_all();

    printf("Starting PAL_test...\n");


    uint sysclockkhz = 300000;
    printf("Setting sysclock to %i khz\n", sysclockkhz);
    set_sys_clock_khz(300000, true);
    uart_set_baudrate(uart0, 115200);
    printf("clock set successfully\n");



    uint8_t sin_lookup[256];
    for(int i = 0; i < ArraySize(sin_lookup); i++)
    {
        sin_lookup[i] = (sin(M_PI*(float(i)/float(ArraySize(sin_lookup)))) * float(ArraySize(sin_lookup)));
    }

    uint8_t lookup[512];
    for(int i = 0; i < 256; i++)
    {
        lookup[i] = i;
    }
    for(int i = 0; i < 256; i++)
    {
        lookup[i+256] = 255-i;
    }


    pio_init(lookup, ArraySize(lookup));

    adc_init();
    adc_set_temp_sensor_enabled(true);

    while(true)
    {
        printf("main core free to vibe\n");
        adc_select_input(4);
        uint16_t v = adc_read();
        float temperature = 27 - ((float(v)*(3.3f / (1 << 12))) - 0.706)/0.001721;
        printf("temperature: %.3fC\n", temperature);
        sleep_ms(1000);
    }

    // while (true) 
    // {
    //     for(int i = 0; i < 64; i++)
    //     {
    //         dac_push_output(*(uint32_t*)(sin_lookup+(i*4)));
    //         //sleep_us(10);
    //     }
    //     //sleep_us(100);
    // }
}
