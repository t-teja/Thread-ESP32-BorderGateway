#pragma once

#include <stddef.h>
#include <stdint.h>

/** "Living Room" -> "living_room". dest always NUL-terminated. */
void hub_slugify(const char *in, char *out, size_t out_len);

/** Build device_id like "th-a1b2c3" from type + MAC suffix. */
void hub_make_device_id(const char *type, const uint8_t mac[6], char *out, size_t out_len);
