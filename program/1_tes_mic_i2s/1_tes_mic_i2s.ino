/*
 * ============================================================
 *  PROGRAM 1: TES MIKROFON INMP441 + ESP32-C3
 * ============================================================
 *  Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)
 *  Board   : ESP32-C3 SuperMini
 *  Sensor  : INMP441 Digital I2S Microphone
 *
 *  FUNGSI  : Membaca data audio digital dari mikrofon INMP441
 *            via protokol I2S, lalu menampilkan amplitudo suara
 *            di Serial Monitor dan Serial Plotter Arduino IDE.
 *            Jika kamu bicara di depan mic, grafik akan bergerak naik-turun.
 *
 *  WIRING (ESP32-C3 SuperMini <-> INMP441):
 *    INMP441 Pin  |  ESP32-C3 Pin  |  Fungsi
 *    -------------|----------------|------------------
 *    VDD          |  3.3V          |  Catu daya
 *    GND          |  GND           |  Ground
 *    SD           |  GPIO 4        |  Serial Data (audio keluar dari mic)
 *    WS           |  GPIO 2        |  Word Select / LRCLK (penanda channel)
 *    SCK          |  GPIO 3        |  Serial Clock (detak sinkronisasi)
 *    L/R          |  GND           |  Pilih channel kiri (mono)
 *
 *  CARA PAKAI:
 *    1. Buka Arduino IDE 2.x
 *    2. Pilih Board: "ESP32C3 Dev Module"
 *    3. Pilih Port: COMxx (sesuai laptopmu)
 *    4. Klik Upload
 *    5. Buka Serial Plotter (Tools > Serial Plotter) pada baud 115200
 *    6. Bicara di depan mic -> grafik akan bergerak!
 * ============================================================
 */

#include <driver/i2s.h>

// ==================== KONFIGURASI PIN I2S ====================
// Sesuaikan pin di bawah ini jika wiring kalian berbeda
#define I2S_WS_PIN    2   // Word Select (LRCLK)
#define I2S_SCK_PIN   3   // Serial Clock (BCLK)
#define I2S_SD_PIN    4   // Serial Data (DOUT dari mic)

// ==================== KONFIGURASI AUDIO ======================
#define SAMPLE_RATE   16000  // Sampling rate 16 kHz (sesuai Teorema Nyquist)
#define I2S_PORT      I2S_NUM_0
#define BUFFER_SIZE   512    // Jumlah sampel per pembacaan

// Buffer untuk menampung data audio mentah dari mikrofon
int32_t raw_samples[BUFFER_SIZE];

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("============================================");
  Serial.println("  TES MIKROFON INMP441 + ESP32-C3");
  Serial.println("  Instrumentasi Cerdas - UGM");
  Serial.println("============================================");
  Serial.println("Buka Serial Plotter untuk melihat grafik suara.");
  Serial.println();

  // ---------- Konfigurasi I2S ----------
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),  // ESP32 sebagai master, mode penerima
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,          // INMP441 output 24-bit, dibaca sebagai 32-bit
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,           // Mono (hanya channel kiri, karena L/R = GND)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,                                     // Jumlah buffer DMA
    .dma_buf_len = BUFFER_SIZE,                             // Ukuran tiap buffer
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // ---------- Konfigurasi Pin I2S ----------
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,     // Serial Clock
    .ws_io_num = I2S_WS_PIN,       // Word Select
    .data_out_num = I2S_PIN_NO_CHANGE,  // Tidak ada output (kita hanya baca dari mic)
    .data_in_num = I2S_SD_PIN      // Serial Data masuk dari mic
  };

  // ---------- Pasang driver I2S ----------
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("GAGAL install driver I2S! Error: %d\n", err);
    while (true) { delay(1000); }  // Berhenti di sini jika gagal
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("GAGAL set pin I2S! Error: %d\n", err);
    while (true) { delay(1000); }
  }

  // Bersihkan buffer DMA agar tidak ada data sisa
  i2s_zero_dma_buffer(I2S_PORT);
  delay(500);

  Serial.println(">> Mikrofon INMP441 siap! Silakan bicara...\n");
}

void loop() {
  size_t bytes_read = 0;

  // Baca data audio dari mikrofon INMP441 via I2S
  i2s_read(I2S_PORT, raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);

  // Hitung jumlah sampel yang berhasil dibaca
  int samples_read = bytes_read / sizeof(int32_t);

  if (samples_read > 0) {
    // ----- Hitung nilai rata-rata (DC Offset) -----
    long long sum = 0;
    for (int i = 0; i < samples_read; i++) {
      // INMP441 mengirim data 24-bit pada 32-bit container, geser kanan 8 bit
      raw_samples[i] = raw_samples[i] >> 8;
      sum += raw_samples[i];
    }
    int32_t dc_offset = sum / samples_read;

    // ----- Hitung amplitudo RMS (Root Mean Square) -----
    // Ini adalah energi suara yang digunakan untuk VAD (Voice Activity Detection)
    double sum_sq = 0;
    int32_t peak_val = 0;
    for (int i = 0; i < samples_read; i++) {
      int32_t val = raw_samples[i] - dc_offset;  // Hilangkan DC Offset
      sum_sq += (double)val * val;
      if (abs(val) > peak_val) peak_val = abs(val);
    }
    double rms = sqrt(sum_sq / samples_read);

    // ----- Tampilkan di Serial Monitor dan Serial Plotter -----
    // Format: RMS,Peak (Serial Plotter akan menggambar 2 garis grafik)
    Serial.printf("RMS:%.0f Peak:%d\n", rms, peak_val);

    // Jika ingin melihat grafik yang lebih bersih di Serial Plotter,
    // uncomment baris di bawah dan comment baris printf di atas:
    // Serial.println((int)rms);
  }
}
