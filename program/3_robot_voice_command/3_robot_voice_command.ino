/*
 * ============================================================
 *  PROGRAM 3: ROBOT VOICE COMMAND (PROGRAM UTAMA)
 * ============================================================
 *  Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)
 *  Board   : ESP32-C3 SuperMini
 *  Sensor  : INMP441 Digital I2S Microphone
 *  Model   : TinyML 1D-CNN INT8 (dari Edge Impulse)
 *
 *  FUNGSI  : Program utama "otak" robot yang:
 *            1. Merekam audio 1 detik dari INMP441
 *            2. Menjalankan model AI TinyML untuk menebak kata perintah
 *            3. Mengeksekusi aksi sesuai hasil tebakan:
 *               - "Maju"   -> LED Hijau ON / Motor maju
 *               - "Mundur"  -> LED Kuning ON / Motor mundur
 *               - "Kanan"  -> LED Biru ON / Motor belok kanan
 *               - "Kiri"   -> LED Merah ON / Motor belok kiri
 *               - "Noise"  -> Semua LED OFF / Robot diam
 *
 *  WIRING:
 *    [Mikrofon INMP441] sama seperti Program 1 & 2
 *    [Indikator LED] (opsional, untuk demo):
 *      LED Hijau  -> GPIO 5  (Maju)
 *      LED Kuning -> GPIO 6  (Mundur)
 *      LED Biru   -> GPIO 7  (Kanan)
 *      LED Merah  -> GPIO 10 (Kiri)
 *      LED Board  -> GPIO 8  (Status inferensi)
 *
 *  PENTING:
 *    Sebelum menggunakan program ini, kamu harus sudah:
 *    1. Melatih model di Edge Impulse (edgeimpulse.com)
 *    2. Mengunduh library Arduino (.zip) dari menu Deployment
 *    3. Menginstal library tersebut di Arduino IDE:
 *       Sketch > Include Library > Add .ZIP Library
 *    4. Ganti nama header di bawah ini sesuai nama proyek Edge Impulse-mu
 * ============================================================
 */

#include <driver/i2s.h>

// ===================================================================
// GANTI BARIS DI BAWAH INI SESUAI NAMA PROYEK EDGE IMPULSE KAMU!
// ===================================================================
// Contoh: Jika nama proyek Edge Impulse kamu adalah "edge-voice-cmd"
// maka headernya menjadi: #include <edge-voice-cmd_inferencing.h>
//
// Uncomment baris di bawah setelah library Edge Impulse terinstall:
// #include <NamaProyekmu_inferencing.h>
// ===================================================================

// ==================== KONFIGURASI PIN I2S ====================
#define I2S_WS_PIN    2
#define I2S_SCK_PIN   3
#define I2S_SD_PIN    4

// ==================== KONFIGURASI PIN LED ====================
// Pin LED untuk indikator visual hasil tebakan AI
// Ganti dengan pin motor driver jika ingin menggerakkan motor robot
#define LED_MAJU      5     // LED Hijau
#define LED_MUNDUR    6     // LED Kuning
#define LED_KANAN     7     // LED Biru
#define LED_KIRI      10    // LED Merah
#define LED_STATUS    8     // LED bawaan ESP32-C3 (indikator proses)

// ==================== KONFIGURASI AUDIO ======================
#define SAMPLE_RATE     16000
#define RECORD_SECONDS  1
#define TOTAL_SAMPLES   (SAMPLE_RATE * RECORD_SECONDS)
#define I2S_PORT        I2S_NUM_0
#define DMA_BUF_LEN     512
#define DMA_BUF_COUNT   4

// ==================== KONFIGURASI AI =========================
#define CONFIDENCE_THRESHOLD  0.80  // Ambang kepercayaan minimum (80%)
                                     // Jika nilai tebakan < 80%, diabaikan (UNKNOWN)

// ==================== KONFIGURASI VAD ========================
#define VAD_RMS_THRESHOLD     500   // Ambang energi RMS untuk Voice Activity Detection
                                     // Jika RMS < nilai ini, dianggap hening (tidak ada suara)

// Buffer audio global
int16_t audio_buffer[TOTAL_SAMPLES];

// Variabel untuk mengukur latensi inferensi
unsigned long inference_start_time = 0;
unsigned long inference_end_time = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup pin LED sebagai output
  pinMode(LED_MAJU,   OUTPUT);
  pinMode(LED_MUNDUR,  OUTPUT);
  pinMode(LED_KANAN,  OUTPUT);
  pinMode(LED_KIRI,   OUTPUT);
  pinMode(LED_STATUS, OUTPUT);

  // Matikan semua LED
  allLedsOff();

  Serial.println("============================================");
  Serial.println("  ROBOT VOICE COMMAND - ESP32-C3 TinyML");
  Serial.println("  Instrumentasi Cerdas - UGM");
  Serial.println("============================================");

  // ---------- Setup I2S ----------
  setupI2S();

  Serial.println(">> Sistem siap! Ucapkan perintah: MAJU / MUNDUR / KANAN / KIRI");
  Serial.println();
}

void loop() {
  // ========== TAHAP 1: REKAM AUDIO 1 DETIK ==========
  bool voice_detected = recordAndDetectVoice();

  if (!voice_detected) {
    // Tidak ada suara terdeteksi (VAD: energi terlalu rendah)
    // Robot tetap diam, hemat komputasi
    delay(100);  // Jeda singkat sebelum cek ulang
    return;
  }

  // ========== TAHAP 2: JALANKAN MODEL AI (INFERENSI) ==========
  Serial.println(">> Suara terdeteksi! Menjalankan AI...");
  digitalWrite(LED_STATUS, HIGH);  // LED status menyala saat AI bekerja

  inference_start_time = millis();

  // --- INFERENSI EDGE IMPULSE ---
  // Uncomment blok kode di bawah setelah library Edge Impulse terinstall:

  /*
  // Siapkan sinyal input untuk Edge Impulse
  signal_t signal;
  signal.total_length = TOTAL_SAMPLES;
  signal.get_data = &get_audio_data;

  // Jalankan inferensi (classifier)
  ei_impulse_result_t result = {0};
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

  inference_end_time = millis();
  unsigned long latency = inference_end_time - inference_start_time;

  if (err != EI_IMPULSE_OK) {
    Serial.printf(">> GAGAL menjalankan inferensi! Error: %d\n", err);
    digitalWrite(LED_STATUS, LOW);
    return;
  }

  // ========== TAHAP 3: TAMPILKAN HASIL TEBAKAN ==========
  Serial.println("--------------------------------------------");
  Serial.printf("  Latensi Inferensi: %lu ms\n", latency);
  Serial.println("  Hasil Prediksi:");

  float max_confidence = 0;
  int max_index = -1;

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.printf("    [%s]: %.2f%%\n",
                  result.classification[ix].label,
                  result.classification[ix].value * 100.0);

    if (result.classification[ix].value > max_confidence) {
      max_confidence = result.classification[ix].value;
      max_index = ix;
    }
  }

  Serial.println("--------------------------------------------");

  // ========== TAHAP 4: EKSEKUSI AKSI ROBOT ==========
  allLedsOff();

  if (max_confidence >= CONFIDENCE_THRESHOLD && max_index >= 0) {
    String label = result.classification[max_index].label;
    Serial.printf(">> PERINTAH: %s (%.1f%%)\n\n", label.c_str(), max_confidence * 100.0);

    if (label == "maju") {
      digitalWrite(LED_MAJU, HIGH);
      // TODO: Tambahkan kode motor maju di sini
      // motor_forward();
    }
    else if (label == "mundur") {
      digitalWrite(LED_MUNDUR, HIGH);
      // TODO: Tambahkan kode motor mundur di sini
      // motor_backward();
    }
    else if (label == "kanan") {
      digitalWrite(LED_KANAN, HIGH);
      // TODO: Tambahkan kode motor belok kanan di sini
      // motor_right();
    }
    else if (label == "kiri") {
      digitalWrite(LED_KIRI, HIGH);
      // TODO: Tambahkan kode motor belok kiri di sini
      // motor_left();
    }
    else {
      // Label "noise" atau lainnya -> robot diam
      Serial.println(">> NOISE/HENING - Robot diam.\n");
    }
  } else {
    Serial.printf(">> UNKNOWN (confidence %.1f%% < threshold %.0f%%)\n\n",
                  max_confidence * 100.0, CONFIDENCE_THRESHOLD * 100.0);
  }
  */

  // --- PLACEHOLDER SEMENTARA (hapus setelah Edge Impulse terinstall) ---
  inference_end_time = millis();
  Serial.printf(">> [PLACEHOLDER] Inferensi selesai dalam %lu ms\n",
                inference_end_time - inference_start_time);
  Serial.println(">> Install library Edge Impulse untuk mengaktifkan AI.\n");
  // --- AKHIR PLACEHOLDER ---

  digitalWrite(LED_STATUS, LOW);

  // Jeda sebelum mendengarkan perintah berikutnya
  delay(500);

  // Matikan LED aksi setelah 1 detik
  delay(500);
  allLedsOff();
}

// ==================== FUNGSI-FUNGSI PENDUKUNG ====================

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
    Serial.printf("GAGAL install driver I2S! Error: %d\n", err);
    while (true) { delay(1000); }
  }

  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
  delay(500);

  Serial.println(">> Driver I2S terpasang.");
}

bool recordAndDetectVoice() {
  /*
   * Merekam audio 1 detik dan mengecek VAD (Voice Activity Detection)
   * Return true jika ada suara manusia, false jika hening.
   */
  int samples_recorded = 0;
  int32_t temp_buffer[DMA_BUF_LEN];
  double sum_sq = 0;

  while (samples_recorded < TOTAL_SAMPLES) {
    size_t bytes_read = 0;
    int to_read = min((int)DMA_BUF_LEN, TOTAL_SAMPLES - samples_recorded);

    i2s_read(I2S_PORT, temp_buffer, to_read * sizeof(int32_t), &bytes_read, portMAX_DELAY);
    int got = bytes_read / sizeof(int32_t);

    for (int i = 0; i < got && samples_recorded < TOTAL_SAMPLES; i++) {
      // Konversi 32-bit -> 16-bit
      int16_t sample = (int16_t)(temp_buffer[i] >> 16);
      audio_buffer[samples_recorded] = sample;
      sum_sq += (double)sample * sample;
      samples_recorded++;
    }
  }

  // Hitung RMS (Root Mean Square) untuk VAD
  double rms = sqrt(sum_sq / TOTAL_SAMPLES);

  if (rms < VAD_RMS_THRESHOLD) {
    // Energi suara terlalu rendah -> hening / noise latar belakang
    return false;
  }

  return true;  // Ada suara manusia!
}

/*
 * Fungsi callback untuk Edge Impulse:
 * Mengambil data audio dari buffer global.
 * Uncomment setelah library Edge Impulse terinstall.
 */
/*
int get_audio_data(size_t offset, size_t length, float *out_ptr) {
  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (float)audio_buffer[offset + i];
  }
  return 0;
}
*/

void allLedsOff() {
  digitalWrite(LED_MAJU,   LOW);
  digitalWrite(LED_MUNDUR,  LOW);
  digitalWrite(LED_KANAN,  LOW);
  digitalWrite(LED_KIRI,   LOW);
  digitalWrite(LED_STATUS, LOW);
}
