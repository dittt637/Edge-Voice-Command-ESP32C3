/*
 * ============================================================
 *  PROGRAM 3C: VOICE COMMAND + OUTPUT SUARA KE SPEAKER (MAX98357A)
 * ============================================================
 *  Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)
 *  Board   : ESP32-C3 SuperMini
 *  Input   : INMP441 Digital I2S Microphone
 *  Output  : MAX98357A I2S Amplifier + Mini Speaker 8 Ohm 0.25W
 *  Model   : TinyML 1D-CNN INT8 (Edge Impulse Arduino Library v1.0.7)
 *
 *  FUNGSI  : 100% OFFLINE di ESP32-C3!
 *            - Mendengarkan suara dari INMP441 (I2S RX)
 *            - Menebak kata perintah ("forward", "left", "right", "stop")
 *            - Otomatis memutar suara balasan ke speaker lewat MAX98357A (I2S TX)
 *              (Misal terdeteksi "forward" -> speaker berbicara "forward")
 *            - Kembali mendengarkan perintah berikutnya.
 *
 *  CATATAN ESP32-C3:
 *    ESP32-C3 hanya punya 1 unit I2S (I2S_NUM_0), sehingga program ini
 *    secara otomatis berganti mode: RX (Mikrofon) <-> TX (Speaker).
 *    Ini juga mencegah mikrofon merekam ulang suara speakernya sendiri.
 *
 *  WIRING MIKROFON INMP441 (TIDAK BERUBAH):
 *    INMP441 Pin  |  ESP32-C3 Pin
 *    -------------|----------------
 *    VDD          |  3.3V
 *    GND          |  GND
 *    WS           |  GPIO 2
 *    SCK          |  GPIO 3
 *    SD           |  GPIO 4
 *    L/R          |  GND
 *
 *  WIRING AMPLIFIER MAX98357A + SPEAKER 8 OHM 0.25W:
 *    MAX98357A Pin |  ESP32-C3 Pin
 *    --------------|----------------
 *    Vin           |  3.3V (disarankan agar aman untuk speaker 0.25W; bisa 5V jika kurang keras)
 *    GND           |  GND
 *    LRC           |  GPIO 5
 *    BCLK          |  GPIO 6
 *    DIN           |  GPIO 7
 *    (+) & (-)     |  Kaki kabel Mini Speaker 8 Ohm 0.25W
 *    (Pin SD dan GAIN pada MAX98357A dibiarkan kosong / tidak disambung)
 * ============================================================
 */

#include <instrumentasi-voice-command_inferencing.h>
#include <driver/i2s.h>
#include <driver/gpio.h>
#include "voice_clips.h"

// ==================== PIN MIKROFON INMP441 (RX) ==============
#define MIC_WS_PIN     2
#define MIC_SCK_PIN    3
#define MIC_SD_PIN     4

// ==================== PIN AMPLIFIER MAX98357A (TX) ===========
#define AMP_LRC_PIN    5
#define AMP_BCLK_PIN   6
#define AMP_DIN_PIN    7

// ==================== KONFIGURASI AUDIO ======================
#define SAMPLE_RATE      16000
#define RECORD_SECONDS   1
#define TOTAL_SAMPLES    (SAMPLE_RATE * RECORD_SECONDS)  // 16000 sampel
#define I2S_PORT         I2S_NUM_0
#define DMA_BUF_LEN      512
#define DMA_BUF_COUNT    4

// ==================== KONFIGURASI AI & SPEAKER ===============
#define CONFIDENCE_THRESHOLD  0.75f  // Ambang batas keyakinan tebakan (75%)
#define VAD_RMS_THRESHOLD     120    // Ambang RMS deteksi suara aktif
#define SPEAKER_VOLUME        0.35f  // Volume speaker 0.10 - 1.00 (aman untuk speaker 0.25W!)

// Buffer audio mentah 1 detik
int16_t audio_buffer[TOTAL_SAMPLES];

// Prototipe fungsi
void setupI2S_Mic();
void stopI2S_All();
void playVoiceClip(const int16_t* clip_progmem, uint32_t length);
bool recordAudioWithVAD();
int get_audio_data(size_t offset, size_t length, float *out_ptr);

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("==================================================");
  Serial.println("  VOICE COMMAND + SPEAKER - ESP32-C3 TinyML       ");
  Serial.println("  Instrumentasi Cerdas - FMIPA UGM                ");
  Serial.println("==================================================");
  Serial.printf("Model : %s v1.0.7\n", EI_CLASSIFIER_PROJECT_NAME);
  Serial.printf("Volume Speaker: %.0f%%\n", SPEAKER_VOLUME * 100.0f);

  // Mulai dalam mode Mendengarkan (Mikrofon INMP441)
  setupI2S_Mic();

  Serial.println(">> SISTEM SIAP! Ucapkan: FORWARD / LEFT / RIGHT / STOP...\n");
}

void loop() {
  // ========== TAHAP 1: REKAM AUDIO & DETEKSI SUARA (VAD) ==========
  bool voice_detected = recordAudioWithVAD();

  if (!voice_detected) {
    delay(50);
    return;
  }

  // ========== TAHAP 2: INFERENSI AI (MFCC + 1D-CNN INT8) ==========
  unsigned long t_start = millis();

  signal_t signal;
  signal.total_length = TOTAL_SAMPLES;
  signal.get_data = &get_audio_data;

  ei_impulse_result_t result = {0};
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

  unsigned long latency = millis() - t_start;

  if (err != EI_IMPULSE_OK) {
    Serial.printf(">> Error inferensi: %d\n", err);
    return;
  }

  // ========== TAHAP 3: CETAK HASIL & PUTAR SUARA KE SPEAKER ==========
  Serial.println("--------------------------------------------------");
  Serial.printf("Waktu Total: %lu ms (MFCC: %d ms | CNN: %d ms)\n",
                latency, result.timing.dsp, result.timing.classification);

  float max_score = 0.0f;
  int max_idx = -1;

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    float score = result.classification[ix].value;
    Serial.printf("  - %-8s : %5.1f%%\n",
                  result.classification[ix].label,
                  score * 100.0f);

    if (score > max_score) {
      max_score = score;
      max_idx = ix;
    }
  }
  Serial.println("--------------------------------------------------");

  if (max_idx >= 0 && max_score >= CONFIDENCE_THRESHOLD) {
    const char* label = result.classification[max_idx].label;
    Serial.printf(">> TERDETEKSI: [%s] (Confidence: %.1f%%)\n",
                  label, max_score * 100.0f);

    if (strcmp(label, "forward") == 0) {
      Serial.println(">> 🔊 Berbicara ke speaker: \"forward\"...\n");
      playVoiceClip(voice_forward, voice_forward_len);
    }
    else if (strcmp(label, "left") == 0) {
      Serial.println(">> 🔊 Berbicara ke speaker: \"left\"...\n");
      playVoiceClip(voice_left, voice_left_len);
    }
    else if (strcmp(label, "right") == 0) {
      Serial.println(">> 🔊 Berbicara ke speaker: \"right\"...\n");
      playVoiceClip(voice_right, voice_right_len);
    }
    else if (strcmp(label, "stop") == 0) {
      Serial.println(">> 🔊 Berbicara ke speaker: \"stop\"...\n");
      playVoiceClip(voice_stop, voice_stop_len);
    }
    else {
      // Kelas "silent" -> tidak perlu mengeluarkan suara
      Serial.println(">> (Silent) Tetap standby.\n");
    }
  } else {
    Serial.printf(">> TIDAK DIKENAL (Tertinggi: %.1f%% < %.0f%%)\n\n",
                  max_score * 100.0f, CONFIDENCE_THRESHOLD * 100.0f);
  }

  delay(400);
}

// ==================== MANAJEMEN DRIVER I2S (RX <-> TX) ====================

void stopI2S_All() {
  i2s_driver_uninstall(I2S_PORT);
  gpio_reset_pin((gpio_num_t)MIC_WS_PIN);
  gpio_reset_pin((gpio_num_t)MIC_SCK_PIN);
  gpio_reset_pin((gpio_num_t)MIC_SD_PIN);
  gpio_reset_pin((gpio_num_t)AMP_LRC_PIN);
  gpio_reset_pin((gpio_num_t)AMP_BCLK_PIN);
  gpio_reset_pin((gpio_num_t)AMP_DIN_PIN);
}

void setupI2S_Mic() {
  // Pastikan pin amplifier dalam kondisi diam agar speaker tidak berdengung saat mic aktif
  pinMode(AMP_DIN_PIN, OUTPUT);
  digitalWrite(AMP_DIN_PIN, LOW);

  i2s_config_t rx_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t rx_pins = {
    .bck_io_num = MIC_SCK_PIN,
    .ws_io_num = MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD_PIN
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &rx_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("GAGAL setup I2S Mic: %d\n", err);
    return;
  }

  i2s_set_pin(I2S_PORT, &rx_pins);
  i2s_zero_dma_buffer(I2S_PORT);
  delay(100);
}

void playVoiceClip(const int16_t* clip_progmem, uint32_t length) {
  // 1. Matikan I2S mode Mic sementara
  stopI2S_All();

  // 2. Pasang I2S mode Speaker (TX) untuk MAX98357A
  i2s_config_t tx_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = VOICE_CLIP_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // Kirim frame stereo agar cocok di semua mode pin SD MAX98357A
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t tx_pins = {
    .bck_io_num = AMP_BCLK_PIN,
    .ws_io_num = AMP_LRC_PIN,
    .data_out_num = AMP_DIN_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &tx_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("GAGAL setup I2S Speaker: %d\n", err);
    setupI2S_Mic();
    return;
  }

  i2s_set_pin(I2S_PORT, &tx_pins);
  i2s_zero_dma_buffer(I2S_PORT);

  // 3. Stream data audio dari Flash (PROGMEM) dalam potongan kecil 256 sampel
  const uint32_t CHUNK = 256;
  int16_t stereo_buf[CHUNK * 2];  // Left + Right = 512 int16 (~1 KB RAM sementara)
  size_t bytes_written = 0;

  for (uint32_t pos = 0; pos < length; pos += CHUNK) {
    uint32_t count = min(CHUNK, length - pos);
    for (uint32_t i = 0; i < count; i++) {
      int16_t raw = (int16_t)pgm_read_word(&clip_progmem[pos + i]);
      int16_t scaled = (int16_t)(raw * SPEAKER_VOLUME);
      stereo_buf[i * 2]     = scaled;  // Left channel
      stereo_buf[i * 2 + 1] = scaled;  // Right channel
    }
    i2s_write(I2S_PORT, stereo_buf, count * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  }

  // 4. Kirim jeda hening singkat agar seluruh sisa audio di DMA keluar tuntas ke speaker
  memset(stereo_buf, 0, sizeof(stereo_buf));
  i2s_write(I2S_PORT, stereo_buf, sizeof(stereo_buf), &bytes_written, portMAX_DELAY);
  i2s_zero_dma_buffer(I2S_PORT);
  delay(80);

  // 5. Kembalikan I2S ke mode Mic (INMP441)
  stopI2S_All();
  setupI2S_Mic();
}

// ==================== AKUISISI AUDIO & VAD ====================

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
    return false;
  }

  Serial.printf(">> Suara aktif (RMS: %.0f)\n", rms);
  return true;
}

int get_audio_data(size_t offset, size_t length, float *out_ptr) {
  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (float)audio_buffer[offset + i];
  }
  return 0;
}
