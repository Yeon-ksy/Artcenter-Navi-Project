#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "SK_WiFiGIGA97F4_2.4G";
const char* password = "1707016740";

AsyncWebServer server(80);

// I2S pins
#define I2S_SCK 32
#define I2S_WS 33
#define I2S_SD 35

const int sample_rate = 16000;
const int buffer_len = 1024;
int16_t i2s_read_buffer[buffer_len];

// WAV header structure
typedef struct {
    char riff[4];            // RIFF chunk descriptor
    uint32_t chunkSize;      // Size of the RIFF chunk
    char wave[4];            // WAVE format descriptor
    char fmt[4];             // "fmt" subchunk descriptor
    uint32_t subchunkSize;   // Size of the "fmt" subchunk
    uint16_t audioFormat;    // Audio format (1 for PCM)
    uint16_t numChannels;    // Number of channels (1 for mono, 2 for stereo)
    uint32_t sampleRate;     // Sample rate
    uint32_t byteRate;       // Byte rate
    uint16_t blockAlign;     // Block align
    uint16_t bitsPerSample;  // Bits per sample
    char data[4];            // "data" subchunk descriptor
    uint32_t dataSize;       // Size of the data subchunk
} wav_header_t;

// WAV header
wav_header_t wav_header = {
    .riff = {'R', 'I', 'F', 'F'},
    .chunkSize = 0, // Update this later
    .wave = {'W', 'A', 'V', 'E'},
    .fmt = {'f', 'm', 't', ' '},
    .subchunkSize = 16,
    .audioFormat = 1,
    .numChannels = 1,
    .sampleRate = sample_rate,
    .byteRate = (sample_rate * 1 * 16) / 8,
    .blockAlign = (1 * 16) / 8,
    .bitsPerSample = 16,
    .data = {'d', 'a', 't', 'a'},
    .dataSize = 0, // Update this later
};

void setup() {
    Serial.begin(115200);
    pinMode(I2S_WS, OUTPUT);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi");
    Serial.print("Server IP: ");
    Serial.println(WiFi.localIP());

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

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<html><body><audio controls autoplay><source src=\"/audio\" type=\"audio/wav\"></audio></body></html>";
        request->send(200, "text/html", html);
        Serial.println("Client requested main page");
    });

    server.on("/audio", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Client requested audio data");
        AsyncWebServerResponse *response = request->beginChunkedResponse("audio/wav", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            size_t bytes_read = 0;
            
            if (index == 0) {
                // Send WAV header on the first request
                memcpy(buffer, &wav_header, sizeof(wav_header));
                bytes_read = sizeof(wav_header);
            } else {
                // Send audio data on subsequent requests
                if (i2s_read(I2S_NUM_0, (char *)buffer, maxLen, &bytes_read, portMAX_DELAY) == ESP_OK) {
                    for (int i = 0; i < bytes_read / 2; i++) {
                        int16_t sample = (buffer[2*i + 1] << 8) | buffer[2*i];
                        int16_t output = sample >> 1;
                        buffer[2*i] = output & 0xFF;
                        buffer[2*i + 1] = output >> 8;
                    }
                    Serial.printf("Transmitted %lu bytes of audio data\n", bytes_read);
                }
            }

            if (bytes_read < maxLen) {
                // Update WAV header with final chunkSize and dataSize
                wav_header.chunkSize = index + bytes_read - 8;
                wav_header.dataSize = index + bytes_read - sizeof(wav_header);
                memcpy(buffer + bytes_read, &wav_header, sizeof(wav_header));
                bytes_read += sizeof(wav_header);
            }

            return bytes_read;
        });
        request->send(response);
    });

    server.begin();
    Serial.println("Web server started");
}

void loop() {
  // Nothing to do here
}
