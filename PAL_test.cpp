#include "hmain.hpp"

int SQ(int v)
{
    return v * v;
}
int distance(int x0, int y0, int x1, int y1)
{
    return sqrt(SQ(x1-x0) + SQ(y1-y0));
}

constexpr float BASE_PERIOD_NS = 3.3333f;
constexpr float LINE_BLANKING_NS = 12500.0f;
constexpr float LINE_SYNC_NS = 4700.0f;
constexpr float FRONT_PORCH_NS = 1650.0f;
constexpr float BACK_PORCH_NS = 5700.0f;
constexpr float VISUAL_NS = 52000.0f;

pio_hw_t* pio;
int sm;
uint dmachan;

void dac_push_output(uint32_t values)
{
    pio_sm_put(pio0, 0, values);
}


void dma_init(pio_hw_t* pio, int sm)
{
    //assert(len % 4 == 0);

    //static uint8_t* lookup_source = source;

    auto dmaDataChan = dma_claim_unused_channel(true);
    auto dmaCtrlChan = dma_claim_unused_channel(true);

    // // Configure the control channel to restart the data channel when it's done
    // auto ctrlChanConfig = dma_channel_get_default_config(dmaCtrlChan);
    // channel_config_set_transfer_data_size(&ctrlChanConfig, DMA_SIZE_32);
    // channel_config_set_read_increment(&ctrlChanConfig, false);
    // channel_config_set_write_increment(&ctrlChanConfig, false);
    // channel_config_set_chain_to(&ctrlChanConfig, dmaDataChan);
    // channel_config_set_irq_quiet(&ctrlChanConfig, true);
    // channel_config_set_high_priority(&ctrlChanConfig, true);
    // channel_config_set_enable(&ctrlChanConfig, true);
    // dma_channel_configure(
    //     dmaCtrlChan, 
    //     &ctrlChanConfig, 
    //     // Write to the read address of the data channel
    //     &dma_hw->ch[dmaDataChan].read_addr,
    //     // Read from the sample points data pointer
    //     &lookup_source,
    //     // One 32-bit word
    //     1,
    //     // Don't start yet
    //     false);

    // // Configure the data channel to output 32 bits at a time to the PIO state machine
    // auto dataChanConfig = dma_channel_get_default_config(dmaDataChan);
    // channel_config_set_transfer_data_size(&dataChanConfig, DMA_SIZE_32);
    // channel_config_set_read_increment(&dataChanConfig, true);
    // channel_config_set_write_increment(&dataChanConfig, false);
    // channel_config_set_dreq(&dataChanConfig, pio_get_dreq(pio, sm, true));
    // channel_config_set_chain_to(&dataChanConfig, dmaCtrlChan);
    // channel_config_set_irq_quiet(&dataChanConfig, true);
    // channel_config_set_high_priority(&dataChanConfig, true);
    // channel_config_set_enable(&dataChanConfig, true);
    // dma_channel_configure(
    //     dmaDataChan,
    //     &dataChanConfig,
    //     // Write to the FIFO
    //     &pio->txf[sm],
    //     // Read from the sample points data
    //     lookup_source,
    //     len / 4,
    //     // Don't start yet
    //     false);

    // dma_start_channel_mask((1u << dmaDataChan) | (1u << dmaCtrlChan));

    //while(true) tight_loop_contents();

    dmachan = dmaDataChan;

    auto dataChanConfig = dma_channel_get_default_config(dmaDataChan);
    channel_config_set_transfer_data_size(&dataChanConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dataChanConfig, true);
    channel_config_set_write_increment(&dataChanConfig, false);
    channel_config_set_dreq(&dataChanConfig, pio_get_dreq(pio, sm, true));
    //channel_config_set_chain_to(&dataChanConfig, dmaCtrlChan);
    channel_config_set_irq_quiet(&dataChanConfig, true);
    channel_config_set_high_priority(&dataChanConfig, true);
    channel_config_set_enable(&dataChanConfig, true);
    dma_channel_configure(
        dmaDataChan,
        &dataChanConfig,
        // Write to the FIFO
        &pio->txf[sm],
        // Read from the sample points data
        NULL,
        0,
        // Don't start yet
        false);
    
    dma_channel_start(dmaDataChan);
    // while(true)
    // {
    //     dma_channel_set_read_addr(dmaDataChan, lookup, true);
    //     dma_channel_wait_for_finish_blocking(dmaDataChan);
    // }
}

void pio_init()
{
    pio = pio0;
    sm = pio_claim_unused_sm(pio0, true);

    uint8_t offset = pio_add_program(pio, &dac_out_program);
    dac_program_init(pio, sm, offset, PIN::DAC_OUT[0], 1.0f);
}

void dac_init()
{
    pio_init();
    dma_init(pio, sm);
}

void dac_write(uint8_t* data, uint len, float div)
{
    assert(len % 4 == 0);

    dma_channel_wait_for_finish_blocking(dmachan);
    pio_sm_set_clkdiv(pio, sm, div);
    dma_channel_set_trans_count(dmachan, len/4, false);
    dma_channel_set_read_addr(dmachan, data, true);
}

#define dac_send_array(a, div) dac_write(a, ArraySize(a), div);

float read_temperature()
{
    adc_select_input(4);
    uint16_t v = adc_read();
    float temperature = 27 - ((float(v)*(3.3f / (1 << 12))) - 0.706)/0.001721;
    return temperature;
}

void write_temperature()
{
    float temp = read_temperature();
    printf("Temperature: %.3f\n", temp);
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


    dac_init();


    adc_init();
    adc_set_temp_sensor_enabled(true);


    const int black = 80;
    const int zero = 0;

    uint8_t long_sync[320] = {0};
    memset(long_sync, 0, ArraySize(long_sync));
    memset(long_sync + 320 - 47 - 1, black, 47);
    uint8_t short_sync[320] = {0};
    memset(short_sync, black, ArraySize(short_sync));
    memset(short_sync, zero, 24);

    const int lines_x = 408;
    const int lines_y = 304;
    const float div = 30;
    const float line_div = 38.23;


    uint8_t video_data[lines_y][lines_x];

    for(int i = 0; i < lines_y; i++)
    {
        for(int ii = 0; ii < lines_x; ii++)
        {
            int dist = distance(i, ii, lines_y/2 + 25, lines_x/2 - 25);
            uint8_t val;
            if(dist < 50)
                val = 255;
            else
                val = black;
            video_data[i][ii] = val;
        }
    }

    uint8_t front_porch[16];
    for(int i = 0; i < ArraySize(front_porch); i++)
    {
        front_porch[i] = black;
    }
    uint8_t line_sync[48] = {0};
    uint8_t back_porch[56];
    for(int i = 0; i < ArraySize(back_porch); i++)
    {
        back_porch[i] = black;
    }

    

    while(true)
    {
        uint64_t tm = get_time_us();
        for(int i = 0; i < 5; i++)
        {
            dac_send_array(long_sync, div);
        }
        for(int i = 0; i < 5; i++)
        {
            dac_send_array(short_sync, div);
        }
        for(int i = 0; i < lines_y; i++)
        {
            dac_send_array(front_porch, div);
            dac_write(line_sync, ArraySize(line_sync), div);
            dac_write(back_porch, ArraySize(back_porch), div);
            dac_write(video_data[i], lines_x, line_div);
        }
        for(int i = 0; i < 6; i++)
        {
            dac_send_array(short_sync, div);
        }
        uint64_t tmdif = (get_time_us()-tm);
        double rtm = double(tmdif) / 1000.0;
        printf("%.4f\n", rtm);
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
