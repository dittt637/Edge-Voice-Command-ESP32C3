/*
 * ============================================================
 *  PROGRAM 3: ROBOT VOICE COMMAND (PROGRAM UTAMA FINAL)
 * ============================================================
 *  Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)
 *  Board   : ESP32-C3 SuperMini
 *  Sensor  : INMP441 Digital I2S Microphone
 *  Model   : TinyML 1D-CNN INT8 (Edge Impulse Arduino Library v1.0.7)
 *
 *  MODEL PARAMETERS:
 *    - Library Header : <instrumentasi-voice-command_inferencing.h>
 *    - Model Labels   : "forward", "left", "right", "silent", "stop"
 *    - Feature Type   : MFCC (330 features)
 *    - Model Size     : ~40 KB (Sangat hemat di SRAM 400 KB ESP32-C3)
 *
 *  WIRING (ESP32-C3 <-> INMP441):
 *    INMP441 Pin  |  ESP32-C3 Pin
 *    -------------|----------------
 *    VDD          |  3.3V
 *    GND          |  GND
 *    SD           |  GPIO 4 (I2S Data In)
 *    WS           |  GPIO 2 (Word Select)
 *    SCK          |  GPIO 3 (Serial Clock)
 *    L/R          |  GND
 *
 *  INDIKATOR AKSI (LED / Driver Motor):
 *    - FORWARD (Maju)  -> GPIO 5  (LED Hijau)
 *    - STOP / MUNDUR   -> GPIO 6  (LED Kuning)
 *    - RIGHT (Kanan)   -> GPIO 7  (LED Biru)
 *    - LEFT (Kiri)     -> GPIO 10 (LED Merah)
 *    - STATUS AI       -> GPIO 8  (LED Board)
 *
 *  CARA PAKAI DI ARDUINO IDE:
 *    1. Install file ZIP library di Arduino IDE:
 *       Sketch > Include Library > Add .ZIP Library...
 *       Pilih file: "ei-instrumentasi-voice-command-arduino-1.0.7-impulse-#1.zip"
 *    2. Buka program ini di Arduino IDE
 *    3. Pilih Board "ESP32C3 Dev Module" pada COM3
 *    4. Klik Upload!
 *    5. Buka Serial Monitor (115200 baud) dan ucapkan kata perintah!
 * ============================================================
 */

#include <instrumentasi-voice-command_inferencing.h>
#include <driver/i2s.h>

// ==================== KONFIGURASI PIN I2S ====================
#define I2S_WS_PIN    2
#define I2S_SCK_PIN   3
#define I2S_SD_PIN    4

// ==================== KONFIGURASI PIN AKSI / LED =============
#define LED_FORWARD   5     // LED Hijau (Maju / Forward)
#define LED_STOP      6     // LED Kuning (Berhenti / Stop)
#define LED_RIGHT     7     // LED Biru (Belok Kanan / Right)
#define LED_LEFT      10    // LED Merah (Belok Kiri / Left)
#define LED_STATUS    8     // LED Board ESP32-C3 (Status AI bekerja)

// ==================== KONFIGURASI AUDIO ======================
#define SAMPLE_RATE     16000
#define RECORD_SECONDS  1
#define TOTAL_SAMPLES   (SAMPLE_RATE * RECORD_SECONDS)  // 16000 sampel
#define I2S_PORT        I2S_NUM_0
#define DMA_BUF_LEN     512
#define DMA_BUF_COUNT   4

// ==================== KONFIGURASI AI =========================
#define CONFIDENCE_THRESHOLD  0.75  // Ambang batas keyakinan tebakan (75%)
#define VAD_RMS_THRESHOLD     120   // Ambang RMS deteksi suara aktif (hening ~20-80, bicara ~200-700)

// Buffer penampung audio mentah 1 detik
int16_t audio_buffer[TOTAL_SAMPLES];

// Prototipe callback audio untuk Edge Impulse
int get_audio_data(size_t offset, size_t length, float *out_ptr);

void setup() {
  Serial.begin(115200);
  delay(1500);

  // Setup pin output LED/Motor
  pinMode(LED_FORWARD, OUTPUT);
  pinMode(LED_STOP,    OUTPUT);
  pinMode(LED_RIGHT,   OUTPUT);
  pinMode(LED_LEFT,    OUTPUT);
  pinMode(LED_STATUS,  OUTPUT);

  allLedsOff();

  Serial.println("==================================================");
  Serial.println("  ROBOT VOICE COMMAND - ESP32-C3 & TinyML (MFCC)  ");
  Serial.println("  Instrumentasi Cerdas - FMIPA UGM                ");
  Serial.println("==================================================");
  Serial.printf("Model: %s v%s\n", EI_CLASSIFIER_PROJECT_NAME, "1.0.7");
  Serial.printf("Label terdaftar: ");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.printf("[%s] ", ei_classifier_inferencing_categories[ix]);
  }
  Serial.println("\n");

  // Inisialisasi I2S untuk mikrofon INMP441
  setupI2S();

  Serial.println(">> SISTEM SIAP! Silakan bicara: FORWARD / LEFT / RIGHT / STOP...\n");
}

void loop() {
  // ========== TAHAP 1: REKAM AUDIO & DETEKSI SUARA (VAD) ==========
  bool voice_detected = recordAudioWithVAD();

  if (!voice_detected) {
    // Ruangan hening -> hemat daya, tidak perlu jalankan AI
    delay(50);
    return;
  }

  // ========== TAHAP 2: JALANKAN INFERENSI AI (TINYML) ==========
  digitalWrite(LED_STATUS, HIGH);  // Indikator AI sedang memproses
  unsigned long t_start = millis();

  // Susun objek sinyal audio untuk pustaka Edge Impulse
  signal_t signal;
  signal.total_length = TOTAL_SAMPLES;
  signal.get_data = &get_audio_data;

  // Jalankan inferensi Edge Impulse (Preprocessing MFCC + 1D-CNN INT8)
  ei_impulse_result_t result = {0};
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

  unsigned long latency = millis() - t_start;
  digitalWrite(LED_STATUS, LOW);

  if (err != EI_IMPULSE_OK) {
    Serial.printf(">> Error inferensi: %d\n", err);
    return;
  }

  // ========== TAHAP 3: CETAK HASIL EVALUASI KE SERIAL ==========
  Serial.println("--------------------------------------------------");
  Serial.printf("⏱️  Latensi Inferensi: %lu ms (DSP MFCC + 1D-CNN)\n", latency);
  Serial.println("📊 Probabilitas Klasifikasi:");

  float max_score = 0.0f;
  int max_idx = -1;

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    float score = result.classification[ix].value;
    Serial.printf("    %-10s: %5.1f%%\n",
                  result.classification[ix].label,
                  score * 100.0f);

    if (score > max_score) {
      max_score = score;
      max_idx = ix;
    }
  }
  Serial.println("--------------------------------------------------");

  // ========== TAHAP 4: EKSEKUSI AKSI ROBOT ==========
  allLedsOff();

  if (max_idx >= 0 && max_score >= CONFIDENCE_THRESHOLD) {
    const char* label = result.classification[max_idx].label;

    if (strcmp(label, "forward") == 0) {
      Serial.printf(">> 🚀 AKSI: [FORWARD / MAJU] (Confidence: %.1f%%)\n\n", max_score * 100.0f);
      digitalWrite(LED_FORWARD, HIGH);
      // motor_forward(); // Aktifkan jika memakai motor driver
    }
    else if (strcmp(label, "left") == 0) {
      Serial.printf(">> ↩️  AKSI: [LEFT / BELOK KIRI] (Confidence: %.1f%%)\n\n", max_score * 100.0f);
      digitalWrite(LED_LEFT, HIGH);
      // motor_left();
    }
    else if (strcmp(label, "right") == 0) {
      Serial.printf(">> ↪️  AKSI: [RIGHT / BELOK KANAN] (Confidence: %.1f%%)\n\n", max_score * 100.0f);
      digitalWrite(LED_RIGHT, HIGH);
      // motor_right();
    }
    else if (strcmp(label, "stop") == 0) {
      Serial.printf(">> 🛑 AKSI: [STOP / BERHENTI] (Confidence: %.1f%%)\n\n", max_score * 100.0f);
      digitalWrite(LED_STOP, HIGH);
      // motor_stop();
    }
    else if (strcmp(label, "silent") == 0) {
      Serial.println(">> 🤫 SUARA HENING (Silent) - Robot standby.\n");
    }
  } else {
    Serial.printf(">> ❓ TIDAK DIKENAL (Confidence tertinggi hanya %.1f%% < %.0f%%)\n\n",
                  max_score * 100.0f, CONFIDENCE_THRESHOLD * 100.0f);
  }

  // Jeda singkat sebelum mendengarkan perintah berikutnya
  delay(600);
}

// ==================== FUNGSI DRIVER I2S & AUDIO ====================

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("GAGAL pasang driver I2S: %d\n", err);
    while (true) delay(1000);
  }

  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
  delay(200);

  Serial.println(">> Driver mikrofon I2S INMP441 terhubung.");
}

bool recordAudioWithVAD() {
  int samples_recorded = 0;
  int32_t temp_buf[DMA_BUF_LEN];
  long long sum = 0;

  while (samples_recorded < TOTAL_SAMPLES) {
    size_t bytes_read = 0;
    int to_read = min((int)DMA_BUF_LEN, TOTAL_SAMPLES - samples_recorded);

    i2s_read(I2S_PORT, temp_buf, to_read * sizeof(int32_t), &bytes_read, portMAX_DELAY);
    int got = bytes_read / sizeof(int32_t);

    for (int i = 0; i < got && samples_recorded < TOTAL_SAMPLES; i++) {
      // Ambil 16-bit teratas (INMP441 24-bit MSB-aligned pada 32-bit container)
      int16_t sample = (int16_t)(temp_buf[i] >> 16);
      audio_buffer[samples_recorded] = sample;
      sum += sample;
      samples_recorded++;
    }
  }

  // Hilangkan DC offset sebelum menghitung energi suara RMS untuk VAD
  int32_t mean = (int32_t)(sum / TOTAL_SAMPLES);
  double sum_sq = 0.0;
  for (int i = 0; i < TOTAL_SAMPLES; i++) {
    double centered = (double)(audio_buffer[i] - mean);
    sum_sq += centered * centered;
  }

  double rms = sqrt(sum_sq / TOTAL_SAMPLES);
  if (rms < VAD_RMS_THRESHOLD) {
    return false;  // Hening / noise latar
  }

  return true;     // Terdeteksi suara ucapan
}

// Callback wajib untuk Edge Impulse: Mengubah buffer audio ke float
int get_audio_data(size_t offset, size_t length, float *out_ptr) {
  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (float)audio_buffer[offset + i];
  }
  return 0;
}

void allLedsOff() {
  digitalWrite(LED_FORWARD, LOW);
  digitalWrite(LED_STOP,    LOW);
  digitalWrite(LED_RIGHT,   LOW);
  digitalWrite(LED_LEFT,    LOW);
  digitalWrite(LED_STATUS,  LOW);
}
