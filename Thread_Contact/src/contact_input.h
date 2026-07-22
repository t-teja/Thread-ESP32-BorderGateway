#pragma once
#include <stdbool.h>
void contact_input_init(void);
/** true = open (magnet away), false = closed */
bool contact_input_is_open(void);
const char *contact_input_state_str(void);
