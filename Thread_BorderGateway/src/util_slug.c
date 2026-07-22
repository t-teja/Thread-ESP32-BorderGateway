#include "util_slug.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "device_types.h"

void hub_slugify(const char *in, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }
    if (!in || !in[0]) {
        snprintf(out, out_len, "unassigned");
        return;
    }
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_len; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c)) {
            out[j++] = (char)tolower(c);
        } else if (c == ' ' || c == '-' || c == '/') {
            if (j > 0 && out[j - 1] != '_') {
                out[j++] = '_';
            }
        }
    }
    while (j > 0 && out[j - 1] == '_') {
        j--;
    }
    out[j] = '\0';
    if (j == 0) {
        snprintf(out, out_len, "unassigned");
    }
}

void hub_make_device_id(const char *type, const uint8_t mac[6], char *out, size_t out_len)
{
    const char *pfx = "dev";
    if (type) {
        if (strcmp(type, HUB_DEVICE_TYPE_TEMP_HUM) == 0) {
            pfx = "th";
        } else if (strcmp(type, HUB_DEVICE_TYPE_CONTACT) == 0) {
            pfx = "ct";
        } else if (strcmp(type, HUB_DEVICE_TYPE_MOTION) == 0) {
            pfx = "mo";
        } else if (strcmp(type, HUB_DEVICE_TYPE_LEAK) == 0) {
            pfx = "lk";
        } else if (strcmp(type, HUB_DEVICE_TYPE_BUTTON) == 0) {
            pfx = "bt";
        }
    }
    snprintf(out, out_len, "%s-%02x%02x%02x", pfx, mac[3], mac[4], mac[5]);
}
