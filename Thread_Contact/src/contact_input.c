#include "contact_input.h"
#include "board.h"
#include "driver/gpio.h"

void contact_input_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONTACT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
}

bool contact_input_is_open(void)
{
    int lvl = gpio_get_level(CONTACT_GPIO);
    /* closed when level matches CONTACT_CLOSED_LEVEL */
    return lvl != CONTACT_CLOSED_LEVEL;
}

const char *contact_input_state_str(void)
{
    return contact_input_is_open() ? "open" : "closed";
}
