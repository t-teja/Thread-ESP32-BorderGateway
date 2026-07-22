/**
 * Tiny captive-portal DNS: reply to any A query with SoftAP IP (default 192.168.4.1).
 */
#include "dns_server.h"

#include <unistd.h>

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "dns";
static TaskHandle_t s_task;
static volatile bool s_run;
static int s_sock = -1;

/* 192.168.4.1 */
static const uint8_t s_answer_ip[4] = {192, 168, 4, 1};

static void dns_task(void *arg)
{
    uint8_t buf[512];
    while (s_run) {
        struct sockaddr_in src = {0};
        socklen_t slen = sizeof(src);
        int len = recvfrom(s_sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &slen);
        if (len < 12) {
            continue;
        }
        /* Keep transaction ID; set response flags */
        buf[2] = 0x81; /* response, recursion available */
        buf[3] = 0x80;
        /* qdcount stays; ancount = 1 */
        buf[6] = 0;
        buf[7] = 1;
        buf[8] = buf[9] = buf[10] = buf[11] = 0;

        int qend = 12;
        while (qend < len && buf[qend] != 0) {
            qend += buf[qend] + 1;
        }
        qend++; /* null */
        if (qend + 4 > len) {
            continue;
        }
        qend += 4; /* type + class */

        /* Append answer: pointer to name at 0x0c, type A, class IN, TTL, RDLENGTH, RDATA */
        int a = qend;
        if (a + 16 > (int)sizeof(buf)) {
            continue;
        }
        buf[a++] = 0xC0;
        buf[a++] = 0x0C;
        buf[a++] = 0x00;
        buf[a++] = 0x01; /* A */
        buf[a++] = 0x00;
        buf[a++] = 0x01; /* IN */
        buf[a++] = 0x00;
        buf[a++] = 0x00;
        buf[a++] = 0x00;
        buf[a++] = 30; /* TTL */
        buf[a++] = 0x00;
        buf[a++] = 0x04;
        memcpy(&buf[a], s_answer_ip, 4);
        a += 4;

        sendto(s_sock, buf, a, 0, (struct sockaddr *)&src, slen);
    }
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
    vTaskDelete(NULL);
}

esp_err_t dns_server_start(void)
{
    if (s_run) {
        return ESP_OK;
    }
    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        return ESP_FAIL;
    }
    int yes = 1;
    setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind :53 failed");
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }
    s_run = true;
    xTaskCreate(dns_task, "dns", 4096, NULL, 5, &s_task);
    ESP_LOGI(TAG, "captive DNS on :53");
    return ESP_OK;
}

void dns_server_stop(void)
{
    s_run = false;
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}
