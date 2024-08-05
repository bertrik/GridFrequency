#include <Arduino.h>

#include "measure.h"

#define SAMPLE_FREQUENCY    5000

// sample buffer
#define BUF_SIZE    2048

static int _pin;
static double _base_frequency;

static uint16_t buffer[BUF_SIZE];
static volatile uint32_t bufr = 0;
static volatile uint32_t bufw = 0;
static int sample_index;
static double sum_i = 0.0;
static double sum_q = 0.0;

static hw_timer_t *timer = nullptr;

static void IRAM_ATTR adc_int(void)
{
    uint16_t latest_reading = analogRead(_pin);
    uint32_t next = (bufw + 1) % BUF_SIZE;
    if (next != bufr) {
        buffer[bufw] = latest_reading;
        bufw = next;
    }
}

void measure_init(int pin, double base_frequency)
{
    analogReadResolution(12);
    analogSetAttenuation(ADC_0db);
    analogSetPinAttenuation(pin, ADC_0db);
    adcAttachPin(pin);

    timer = timerBegin(0, 80, true);

    bufr = 0;
    bufw = 0;
    sample_index = 0;
    
    _pin = pin;
    _base_frequency = base_frequency;
}

void measure_start(void)
{
    timerAttachInterrupt(timer, &adc_int, true);
    timerAlarmWrite(timer, 1000000 / SAMPLE_FREQUENCY, true);   // 5kHz timer
    timerAlarmEnable(timer);
}

static bool measure_get(uint16_t &val)
{
    if (bufr == bufw) {
        return false;
    }
    int next = (bufr + 1) % BUF_SIZE;
    val = buffer[bufr];
    bufr = next;
    return true;
}

// true means that a result is ready
bool measure_loop(double &angle, double &ampl)
{
    uint16_t value;
    while (measure_get(value)) {
        sum_i += value * cos(2.0 * M_PI * _base_frequency * sample_index / SAMPLE_FREQUENCY);
        sum_q += value * sin(2.0 * M_PI * _base_frequency * sample_index / SAMPLE_FREQUENCY);
        sample_index++;
        if (sample_index == SAMPLE_FREQUENCY) {
            angle = 180.0 * atan2(sum_i, sum_q) / M_PI;
            ampl = hypot(sum_i, sum_q) / SAMPLE_FREQUENCY;
            sample_index = 0;
            sum_i = 0.0;
            sum_q = 0.0;
            return true;
        }
    }
    return false;
}


