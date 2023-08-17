#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <driver/adc.h>

// I2S pins
#define I2S_SCK 32
#define I2S_WS 33
#define I2S_SD 35

const int sample_rate = 16000;
const int buffer_len = 1024;
int16_t i2s_read_buffer[buffer_len];

void setup() {
  // Setup Serial Monitor
  Serial.begin(115200);

  // I2S configuration
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sample_rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,
      .dma_buf_len = buffer_len,
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void loop() {
  size_t bytes_read = 0;
  if (i2s_read(I2S_NUM_0, (char *)i2s_read_buffer, buffer_len * 2, &bytes_read, portMAX_DELAY) == ESP_OK) {
    for (int i = 0; i < bytes_read / 2; i++) {
      Serial.print(i2s_read_buffer[i]);
      Serial.print(", ");
    }
    Serial.println();
  }

  delay(500); // Print values every 500ms
}