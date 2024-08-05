#include <stdbool.h>

void measure_init(int pin, double base_frequency);
void measure_start(void);
bool measure_loop(double &angle, double &ampl);


