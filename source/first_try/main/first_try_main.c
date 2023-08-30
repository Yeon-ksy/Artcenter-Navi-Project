#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy the header value string */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            printf("Found header => Host: %s\n", buf);
        }
        free(buf);
    }

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

httpd_uri_t uri_handler = {
	.uri       = "/",
	.method	  = HTTP_GET,
	.handler   = hello_get_handler,
	.user_ctx  = "<h2>Hello from your esp32!</h2>"
};

void start_webserver(void)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	if (httpd_start(&server, &config) == ESP_OK) {
	    httpd_register_uri_handler(server, &uri_handler);
	}
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

void wifi_init_sta()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid="pinklab",
            .password="pinkwink",
        },
    };

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    tcpip_adapter_ip_info_t ip_info;
    vTaskDelay(10000 / portTICK_PERIOD_MS);  // wait for connection to establish
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info) == 0) {
        printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
        printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));
        printf("Netmask:     %s\n", ip4addr_ntoa(&ip_info.netmask));
    }
}    

void app_main()
{
	nvs_flash_init();
	wifi_init_sta();
	start_webserver();
}