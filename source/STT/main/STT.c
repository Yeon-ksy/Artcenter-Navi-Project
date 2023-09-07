#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "filter_resample.h"
#include "input_key_service.h"
#include "audio_idf_version.h"
#include "nvs_flash.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#define CONFIG_WIFI_SSID "pinklab"      
#define CONFIG_WIFI_PASSWORD "pinkwink"
#define CONFIG_SERVER_URI "http://192.168.0.19:8000/upload"  

#define EXAMPLE_AUDIO_SAMPLE_RATE  (16000)
#define EXAMPLE_AUDIO_BITS         (16)
#define EXAMPLE_AUDIO_CHANNELS     (1)

static const char *TAG = "AUDIO_APP";

// Global variables for pipeline and elements
audio_pipeline_handle_t record_pipeline, playback_pipeline;
audio_element_handle_t i2s_stream_reader, i2s_stream_writer;
audio_element_handle_t http_stream_writer, http_stream_reader;
audio_element_handle_t mp3_decoder;

// State management
typedef enum {
    APP_STATE_IDLE,
    APP_STATE_RECORDING,
    APP_STATE_PLAYBACK
} app_state_t;

app_state_t app_state = APP_STATE_IDLE;

// Wi-Fi 이벤트 핸들러 추가
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
}

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    ESP_LOGI(TAG, "[HTTP Event] Event ID: %d", msg->event_id);
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    char len_buf[16];
    static int total_write = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // set header
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        char dat[10] = {0};
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_SAMPLE_RATE);
        esp_http_client_set_header(http, "x-audio-sample-rates", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_BITS);
        esp_http_client_set_header(http, "x-audio-bits", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_CHANNELS);
        esp_http_client_set_header(http, "x-audio-channel", dat);
        total_write = 0;
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
        // write data
        int wlen = sprintf(len_buf, "%x\r\n", msg->buffer_len);
        if (esp_http_client_write(http, len_buf, wlen) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, msg->buffer, msg->buffer_len) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, "\r\n", 2) <= 0) {
            return ESP_FAIL;
        }
        total_write += msg->buffer_len;
        printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
        return msg->buffer_len;
    }

    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
        if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST");
        char *buf = calloc(1, 64);
        assert(buf);
        int read_len = esp_http_client_read(http, buf, 64);
        if (read_len <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        buf[read_len] = 0;
        ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)buf);
        free(buf);
        return ESP_OK;
    }
    return ESP_OK;
}

// Event handler for button events
static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
                if (app_state == APP_STATE_IDLE) {
                    ESP_LOGI(TAG, "[ * ] [Rec] Start recording...");
                     // 서버 접근성 확인 로그
                    ESP_LOGI(TAG, "Attempting to connect to server: %s", CONFIG_SERVER_URI);
                    app_state = APP_STATE_RECORDING;
                    // URI 설정
                    audio_element_set_uri(http_stream_writer, CONFIG_SERVER_URI);

                    audio_pipeline_run(record_pipeline);
                }
                break;
            case INPUT_KEY_USER_ID_PLAY:
                if (app_state == APP_STATE_IDLE) {
                    ESP_LOGI(TAG, "[ * ] [Play] Start playback...");
                    app_state = APP_STATE_PLAYBACK;
                    audio_pipeline_run(playback_pipeline);
                }
                break;
        }
    } else if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
                if (app_state == APP_STATE_RECORDING) {
                    ESP_LOGI(TAG, "[ * ] [Rec] Stop recording...");
                    app_state = APP_STATE_IDLE;
                    audio_pipeline_stop(record_pipeline);
                    audio_pipeline_wait_for_stop(record_pipeline);
                }
                break;
            case INPUT_KEY_USER_ID_PLAY:
                if (app_state == APP_STATE_PLAYBACK) {
                    ESP_LOGI(TAG, "[ * ] [Play] Stop playback...");
                    app_state = APP_STATE_IDLE;
                    audio_pipeline_stop(playback_pipeline);
                    audio_pipeline_wait_for_stop(playback_pipeline);
                }
                break;
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Starting app main");  // 로그 추가
    // 메모리 사용량 확인
    ESP_LOGI(TAG, "Initial free heap size: %d bytes", esp_get_free_heap_size());

    // Initialize NVS for Wi-Fi configuration storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // TCP/IP 스택 초기화
    esp_netif_init();

    // Wi-Fi 설정 구조체 초기화
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    // Wi-Fi 모듈 초기화
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Wi-Fi 모드 설정
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Wi-Fi 시작
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wi-Fi 이벤트 핸들러 등록
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // Initialize peripherals and Wi-Fi
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    // Initialize Button peripheral
    audio_board_key_init(set);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, NULL);

    // Initialize audio elements and pipelines
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    record_pipeline = audio_pipeline_init(&pipeline_cfg);
    playback_pipeline = audio_pipeline_init(&pipeline_cfg);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.type = AUDIO_STREAM_WRITER;
    http_cfg.event_handle = _http_stream_event_handle;
    http_stream_writer = http_stream_init(&http_cfg);
    http_stream_reader = http_stream_init(&http_cfg);

    // Initialize I2S stream reader and writer
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.out_rb_size = 16 * 1024; // ring buffer 크기 설정
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    // Initialize MP3 decoder
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    // Register elements to recording pipeline
    audio_pipeline_register(record_pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(record_pipeline, http_stream_writer, "http");

    // Link elements together [I2S]-->http_stream-->[HTTP Server]
    const char *link_tag_record[2] = {"i2s", "http"};
    audio_pipeline_link(record_pipeline, &link_tag_record[0], 2);

    // Register elements to playback pipeline
    audio_pipeline_register(playback_pipeline, http_stream_reader, "http");
    audio_pipeline_register(playback_pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(playback_pipeline, i2s_stream_writer, "i2s");

    // Link elements together [HTTP]-->mp3_decoder-->i2s_stream-->[I2S]
    const char *link_tag_playback[3] = {"http", "mp3", "i2s"};
    audio_pipeline_link(playback_pipeline, &link_tag_playback[0], 3);

    // Create an event interface
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    // Listen for pipeline events
    audio_pipeline_set_listener(record_pipeline, evt);
    audio_pipeline_set_listener(playback_pipeline, evt);

    while (1) {
        ESP_LOGI(TAG, "[APP] Main loop iteration");  // 로그 추가

        // 메모리 사용량 확인
        ESP_LOGI(TAG, "Current free heap size: %d bytes", esp_get_free_heap_size());

        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        ESP_LOGI(TAG, "[APP] Received message from source type: %d, cmd: %d", msg.source_type, msg.cmd);  // 로그 추가
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
            if (msg.source == (void *) mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                audio_element_info_t music_info = {0};
                audio_element_getinfo(mp3_decoder, &music_info);
                ESP_LOGI(TAG, "[ * ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);
                i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            }

            if (msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                if ((int)msg.data == AEL_STATUS_STATE_STOPPED) {
                    ESP_LOGW(TAG, "[ * ] Stop event received");
                    if (app_state == APP_STATE_RECORDING) {
                        app_state = APP_STATE_IDLE;
                        audio_pipeline_stop(record_pipeline);
                        audio_pipeline_wait_for_stop(record_pipeline);
                    } else if (app_state == APP_STATE_PLAYBACK) {
                        app_state = APP_STATE_IDLE;
                        audio_pipeline_stop(playback_pipeline);
                        audio_pipeline_wait_for_stop(playback_pipeline);
                    }
                }
            }
        }
    }
}