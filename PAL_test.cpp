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


constexpr int black = 80;
constexpr int zero = 0;

constexpr int lines_x = 408;
constexpr int lines_y = 304;
constexpr int samples_per_pixel = 2;
constexpr int send_buffer_size = lines_x*samples_per_pixel;

pio_hw_t* pio;
int sm;
uint dmachan;


uint8_t video_data[lines_y][lines_x] = {0};
uint8_t color_data[lines_y][lines_x/2] = {0};
uint8_t send_buffer[send_buffer_size] = {0};


uint64_t cmptm = 0;
uint64_t cmptm_tot = 0;
uint64_t cmptm_max = 0;


void WriteColor(int x, int y, uint8_t val)
{
    assert(val <= 15);

    uint8_t cv = color_data[y][x/2];
    if(x & 0b1)
    {
        cv = (cv & 0b11110000) | val;
    }
    else
    {
        cv = (cv & 0b00001111) | (val << 4);
    }
    color_data[y][x/2] = cv;
}
uint8_t ReadColor(int x, int y)
{
    uint8_t cv = color_data[y][x/2];
    if(x & 0b1)
    {
        return (cv & 0b00001111);
    }
    else
    {
        return (cv & 0b11110000) >> 4;
    }
}
void ComputeSendBuffer(int line)
{
    cmptm = get_time_us();

    for(int i = 0; i < send_buffer_size; i++)
    {
        uint8_t v = video_data[line][i/samples_per_pixel];
        uint8_t color = ReadColor(i/samples_per_pixel, line);
        if(color == 1)
            v = black;
        else if(v == black && color == 2)
            v = black+100;
        send_buffer[i] = v;

    }

    cmptm = get_time_us() - cmptm;
    cmptm_tot += cmptm;
    if(cmptm > cmptm_max)
        cmptm_max = cmptm;
}

#define PIN_COUNT 8
static inline void dac_program_init(PIO pio, uint sm, uint offset, uint pin_base, float divider) 
{
    for(uint i=pin_base; i<pin_base+PIN_COUNT; i++) 
    {
        pio_gpio_init(pio, i);
    }
    pio_sm_set_consecutive_pindirs(pio, sm, pin_base, PIN_COUNT, true);

    pio_sm_config c = dac_out_program_get_default_config(offset); 

    sm_config_set_out_shift(&c, true, true, 32); // true - shift right, auto pull, # of bits

    sm_config_set_out_pins(&c, pin_base, PIN_COUNT);

    sm_config_set_clkdiv(&c, divider);

    // join the FIFO buffers to get more DMA throughput?
    // we use the transmit only so join RX to the TX?
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    pio_sm_clear_fifos(pio, sm);
}

void dac_push_output(uint32_t values)
{
    pio_sm_put(pio0, 0, values);
}


void dma_init(pio_hw_t* pio, int sm)
{

    dmachan = dma_claim_unused_channel(true);    

    auto c = dma_channel_get_default_config(dmachan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    channel_config_set_irq_quiet(&c, true);
    channel_config_set_high_priority(&c, true);
    channel_config_set_enable(&c, true);
    dma_channel_configure(
        dmachan,
        &c,
        // Write to the FIFO
        &pio->txf[sm],
        NULL,
        0,
        false);
    
    dma_channel_start(dmachan);
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


    uint8_t long_sync[320] = {0};
    memset(long_sync, 0, ArraySize(long_sync));
    memset(long_sync + 320 - 47 - 1, black, 47);
    uint8_t short_sync[320] = {0};
    memset(short_sync, black, ArraySize(short_sync));
    memset(short_sync, zero, 24);





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
            if(dist < 25)
                WriteColor(ii, i, 1);
            else if(dist < 100)
                WriteColor(ii, i, 2);
            else
                WriteColor(ii, i, 4);
        }
    }
    struct
    {
        uint8_t front_porch[16];
        uint8_t line_sync[48] = {0};
        uint8_t back_porch[56];
    }fb = {0};
    for(int i = 0; i < ArraySize(fb.front_porch); i++)
    {
        fb.front_porch[i] = black;
    }
    for(int i = 0; i < ArraySize(fb.back_porch); i++)
    {
        fb.back_porch[i] = black;
    }

    
const float div = 30;
const float line_div = 38.23/samples_per_pixel;//9.5588331;

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
        ComputeSendBuffer(0);
        for(int i = 0; i < lines_y; i++)
        {
            // dac_send_array(front_porch, div);
            // dac_write(line_sync, ArraySize(line_sync), div);
            // dac_write(back_porch, ArraySize(back_porch), div);
            dac_write((uint8_t*)&fb, sizeof(fb), div);
            
            dac_write(send_buffer, send_buffer_size, line_div);

            if(i != lines_y-1)
                ComputeSendBuffer(i+1);

            //dac_write(video_data[i], lines_x, line_div*samples_per_pixel);
        }
        for(int i = 0; i < 6; i++)
        {
            dac_send_array(short_sync, div);
        }
        uint64_t tmdif = (get_time_us()-tm);
        double rtm = double(tmdif) / 1000.0;
        float average = float(cmptm_tot) / float(lines_y);
        printf("%.4f %lli %lli %lli %.3f\n", rtm, cmptm, cmptm_max, cmptm_tot, average);
        cmptm_tot = 0;
        cmptm_max = 0;
    }
}
