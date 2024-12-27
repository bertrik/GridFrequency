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

static double *sine_table;
static size_t sine_table_len;

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

static double sine(int index)
{
    index = index % sine_table_len;
    return sine_table[index];
}

static double cosine(int index)
{
    return sine(index + sine_table_len / 4);
}

void measure_init(int pin, double base_frequency)
{
    _pin = pin;
    _base_frequency = base_frequency;

    bufr = 0;
    bufw = 0;
    sample_index = 0;

    // pre-calculate sine table
    sine_table_len = SAMPLE_FREQUENCY / _base_frequency;
    sine_table = new double[sine_table_len];
    for (int i = 0; i < sine_table_len; i++) {
        sine_table[i] = sin(2.0 * M_PI * i / sine_table_len);
    }

    analogReadResolution(12);
    analogSetAttenuation(ADC_0db);
    analogSetPinAttenuation(pin, ADC_0db);
    adcAttachPin(pin);
}

void measure_start(void)
{
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &adc_int, true);
    timerAlarmWrite(timer, 1000000 / SAMPLE_FREQUENCY, true);   // 5kHz timer
    timerAlarmEnable(timer);
}

void measure_stop(void)
{
    if (timerAlarmEnabled(timer)) {
        timerAlarmDisable(timer);
        timerDetachInterrupt(timer);
        timerEnd(timer);
    }
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
        sum_i += value * cosine(sample_index);
        sum_q += value * sine(sample_index);
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


