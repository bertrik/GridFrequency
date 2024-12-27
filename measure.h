#include <stdbool.h>

void measure_init(int pin, double base_frequency);
void measure_start(void);
void measure_stop(void);
bool measure_loop(double &angle, double &ampl);


