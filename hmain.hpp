#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/interp.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/adc.h"

#include "redpicolib/RedPicoLib.hpp"
#include "dac_out.pio.h"

namespace PIN
{
    uint8_t DAC_OUT[8] = {2, 3, 4, 5, 6, 7, 8, 9};
}