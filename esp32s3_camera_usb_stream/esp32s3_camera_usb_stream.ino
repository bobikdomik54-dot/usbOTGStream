#include "esp_camera.h"

#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40
#define SIOC_GPIO_NUM   39
#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     11
#define Y7_GPIO_NUM     12
#define Y6_GPIO_NUM     14
#define Y5_GPIO_NUM     16
#define Y4_GPIO_NUM     18
#define Y3_GPIO_NUM     17
#define Y2_GPIO_NUM     15
#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

static const uint8_t FRAME_START[2] = {0xFF, 0xAA};
static const uint8_t FRAME_END[2]   = {0xFF, 0xBB};

#define CHUNK_SIZE              512
#define FRAME_INTERVAL_MS       33
#define CAMERA_INIT_ATTEMPTS    5
#define CAMERA_INIT_RETRY_MS    200

static unsigned long lastFrameMs = 0;

static bool initCamera();
static void sendFrame(const uint8_t *buf, uint32_t len);

static void cameraHwReset() {
  if (PWDN_GPIO_NUM >= 0) {
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, HIGH);
    delay(10);
    digitalWrite(PWDN_GPIO_NUM, LOW);
    delay(10);
  }
  if (RESET_GPIO_NUM >= 0) {
    pinMode(RESET_GPIO_NUM, OUTPUT);
    digitalWrite(RESET_GPIO_NUM, LOW);
    delay(10);
    digitalWrite(RESET_GPIO_NUM, HIGH);
    delay(10);
  }
}

void setup() {
  Serial.begin(0);
  delay(500);

  if (!initCamera()) {
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
    }
  }
}

void loop() {
  if (!Serial) {
    delay(100);
    return;
  }

  unsigned long now = millis();
  if (now - lastFrameMs < FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = now;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    return;
  }

  sendFrame(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

static bool initCamera() {
  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 16000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count     = 2;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  bool ok = false;
  for (int attempt = 0; attempt < CAMERA_INIT_ATTEMPTS && !ok; attempt++) {
    cameraHwReset();
    if (esp_camera_init(&config) == ESP_OK) {
      ok = true;
    } else {
      esp_camera_deinit();
      delay(CAMERA_INIT_RETRY_MS);
    }
  }
  if (!ok) {
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_quality(s, 10);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_aec_value(s, 300);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)2);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_denoise(s, 1);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_sharpness(s, 0);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_colorbar(s, 0);
    s->set_dcw(s, 1);
    s->set_special_effect(s, 0);
  }

  return true;
}

static void sendFrame(const uint8_t *buf, uint32_t len) {
  uint8_t header[6];
  header[0] = FRAME_START[0];
  header[1] = FRAME_START[1];
  header[2] = (uint8_t)((len >>  0) & 0xFF);
  header[3] = (uint8_t)((len >>  8) & 0xFF);
  header[4] = (uint8_t)((len >> 16) & 0xFF);
  header[5] = (uint8_t)((len >> 24) & 0xFF);
  Serial.write(header, sizeof(header));

  uint32_t offset = 0;
  while (offset < len) {
    uint32_t chunk = (len - offset < CHUNK_SIZE) ? (len - offset) : CHUNK_SIZE;
    Serial.write(buf + offset, chunk);
    offset += chunk;
  }

  Serial.write(FRAME_END, sizeof(FRAME_END));
}
