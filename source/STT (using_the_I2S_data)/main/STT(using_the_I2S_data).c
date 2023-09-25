#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "filter_resample.h"
#include "input_key_service.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "REC_RAW_HTTP";

bool is_recording = false;

#define EXAMPLE_AUDIO_SAMPLE_RATE  (16000)
#define EXAMPLE_AUDIO_BITS         (16)
#define EXAMPLE_AUDIO_CHANNELS     (1)

#define DEMO_EXIT_BIT (BIT0)

#define CONFIG_WIFI_SSID "pinklab"        // Replace with your actual WiFi SSID
#define CONFIG_WIFI_PASSWORD "pinkwink"  // Replace with your actual WiFi password
#define CONFIG_SERVER_URI "http://192.168.0.19:8000/upload"

static EventGroupHandle_t EXIT_FLAG;

audio_pipeline_handle_t pipeline;
audio_element_handle_t i2s_stream_reader;
audio_element_handle_t http_stream_writer;

uint8_t audio_data[512];
size_t bytes_read;

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
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

float calculate_audio_level(uint8_t *audio_data, size_t size) {
    float sum = 0.0;
    int16_t *samples = (int16_t *)audio_data;  // 16-bit samples
    size_t num_samples = size / sizeof(int16_t);  // Calculating the number of samples

    for (size_t i = 0; i < num_samples; ++i) {
        sum += samples[i] * samples[i];
    }

    float mean = sum / num_samples;  
    float rms = sqrt(mean); 

    return rms;
}

void check_audio_level_and_record(uint8_t *audio_data, size_t size) {
    float level = calculate_audio_level(audio_data, size);
<<<<<<< HEAD
    ESP_LOGI("AudioLevel", "Calculated audio level: %f", level);
    const float THRESHOLD_START = 1500;  // 시작 임계값
    const float THRESHOLD_STOP = 300;   // 중지 임계값
=======
    const float THRESHOLD_START = 0.5;  // Start Threshold
    const float THRESHOLD_STOP = 0.2;   // Stop Threshold
>>>>>>> parent of 33a0248 (STT(using_the_button) - ing)

    if (level > THRESHOLD_START && !is_recording) {
        // Start recording
        audio_pipeline_run(pipeline);
        is_recording = true;
    } else if (level < THRESHOLD_STOP && is_recording) {
        // Stop recording
        audio_pipeline_stop(pipeline);
<<<<<<< HEAD
        audio_pipeline_wait_for_stop(pipeline);
        audio_pipeline_reset_ringbuffer(pipeline);
        audio_pipeline_reset_elements(pipeline);

        audio_element_set_uri(http_stream_writer, CONFIG_SERVER_URI);
        ESP_LOGI("PipelineStatus", "Audio pipeline stopped"); 
=======
>>>>>>> parent of 33a0248 (STT(using_the_button) - ing)
        is_recording = false;
    }
}


void app_main(void)
{
<<<<<<< HEAD
    esp_log_level_set("AudioLevel", ESP_LOG_INFO);
    esp_log_level_set("RecordingStatus", ESP_LOG_INFO);
=======
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
>>>>>>> parent of 33a0248 (STT(using_the_button) - ing)

    EXIT_FLAG = xEventGroupCreate();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[ 1 ] Initialize Button Peripheral & Connect to wifi network");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    // Start wifi & button peripheral
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for recs_stream_readerording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create http stream to post data to server");
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.type = AUDIO_STREAM_WRITER;
    http_cfg.event_handle = _http_stream_event_handle;
    http_stream_writer = http_stream_init(&http_cfg);
    audio_element_set_uri(http_stream_writer, CONFIG_SERVER_URI);  // URI 설정
    
    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.out_rb_size = 16 * 1024; // Increase buffer to avoid missing data in bad network conditions
<<<<<<< HEAD
    i2s_cfg.i2s_port = I2S_NUM_1;
=======
    i2s_cfg.i2s_port = I2S_NUM_0;
>>>>>>> parent of 33a0248 (STT(using_the_button) - ing)
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);


    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, http_stream_writer, "http");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]");
    const char *link_tag[2] = {"i2s", "http"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    i2s_stream_set_clk(i2s_stream_reader, EXAMPLE_AUDIO_SAMPLE_RATE, EXAMPLE_AUDIO_BITS, EXAMPLE_AUDIO_CHANNELS);

    while (1) {
<<<<<<< HEAD
        // I2S에서 오디오 데이터 읽기
        i2s_read(I2S_NUM_1, audio_data, sizeof(audio_data), &bytes_read, portMAX_DELAY);
        ESP_LOGI("AudioData", "First 10 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            audio_data[0], audio_data[1], audio_data[2], audio_data[3],
            audio_data[4], audio_data[5], audio_data[6], audio_data[7],
            audio_data[8], audio_data[9]);
        ESP_LOGI("I2SRead", "Bytes read from I2S: %zu", bytes_read);
=======
        // Reading audio data from I2S
        uint8_t audio_data[1024];
        size_t bytes_read;

        i2s_read(I2S_NUM_0, audio_data, sizeof(audio_data), &bytes_read, portMAX_DELAY);
>>>>>>> parent of 33a0248 (STT(using_the_button) - ing)

        // Audio level check and start/stop recording
        check_audio_level_and_record(audio_data, bytes_read);
        }

    // ESP_LOGI(TAG, "[ 4 ] Press [Rec] button to record, Press [Mode] to exit");
    // xEventGroupWaitBits(EXIT_FLAG, DEMO_EXIT_BIT, true, false, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 5 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, http_stream_writer);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_writer);
    audio_element_deinit(i2s_stream_reader);
    esp_periph_set_destroy(set);
    
}
