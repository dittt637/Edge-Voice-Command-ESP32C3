/*
 * ============================================================
 *  PROGRAM 2: PEREKAM DATASET SUARA (DATA LOGGER)
 * ============================================================
 *  Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)
 *  Board   : ESP32-C3 SuperMini
 *  Sensor  : INMP441 Digital I2S Microphone
 *
 *  FUNGSI  : Merekam audio 1 detik dari mikrofon INMP441 setiap kali
 *            tombol BOOT ditekan (atau perintah Serial dikirim),
 *            lalu mengirim data PCM mentah ke laptop via Serial.
 *            Data tersebut ditangkap oleh skrip Python pendamping
 *            (capture_wav.py) dan disimpan sebagai file WAV 16 kHz.
 *
 *  WIRING  : Sama dengan Program 1 (tes_mic_i2s)
 *    INMP441 Pin  |  ESP32-C3 Pin
 *    -------------|---------------
 *    VDD          |  3.3V
 *    GND          |  GND
 *    SD           |  GPIO 4
 *    WS           |  GPIO 2
 *    SCK          |  GPIO 3
 *    L/R          |  GND
 *
 *  CARA PAKAI:
 *    1. Upload program ini ke ESP32-C3
 *    2. Jalankan skrip Python pendamping: python capture_wav.py
 *    3. Di skrip Python, ketik label kelas (misal: maju)
 *    4. Tekan tombol BOOT di ESP32-C3 (atau kirim 'r' lewat Serial)
 *    5. Ucapkan kata perintah saat LED menyala
 *    6. File WAV otomatis tersimpan di laptop
 * ============================================================
 */

#include <driver/i2s.h>

// ==================== KONFIGURASI PIN I2S ====================
#define I2S_WS_PIN    2
#define I2S_SCK_PIN   3
#define I2S_SD_PIN    4

// ==================== KONFIGURASI AUDIO ======================
#define SAMPLE_RATE     16000   // 16 kHz
#define RECORD_SECONDS  1       // Durasi rekaman 1 detik
#define TOTAL_SAMPLES   (SAMPLE_RATE * RECORD_SECONDS)  // = 16000 sampel
#define I2S_PORT        I2S_NUM_0
#define DMA_BUF_LEN     512
#define DMA_BUF_COUNT   4

// ==================== KONFIGURASI TOMBOL =====================
#define BOOT_BUTTON_PIN 9   // Tombol BOOT pada ESP32-C3 SuperMini (GPIO 9)
#define LED_PIN         8   // LED bawaan pada ESP32-C3 SuperMini (GPIO 8)

// Buffer audio: menyimpan 1 detik rekaman (16000 sampel x 2 byte = 32 KB)
int16_t audio_buffer[TOTAL_SAMPLES];

// Variabel status
bool recording = false;
unsigned long last_press = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup tombol dan LED
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("============================================");
  Serial.println("  PEREKAM DATASET SUARA - ESP32-C3");
  Serial.println("  Instrumentasi Cerdas - UGM");
  Serial.println("============================================");
  Serial.println("Tekan tombol BOOT atau kirim 'r' untuk mulai merekam.");
  Serial.println();

  // ---------- Konfigurasi I2S ----------
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

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
  delay(500);

  Serial.println(">> Mikrofon siap. Menunggu perintah rekam...");
}

void loop() {
  // Cek apakah tombol BOOT ditekan (active LOW) atau perintah Serial 'r' diterima
  bool trigger = false;

  // Cek tombol BOOT (dengan debounce 500ms)
  if (digitalRead(BOOT_BUTTON_PIN) == LOW && (millis() - last_press > 500)) {
    last_press = millis();
    trigger = true;
  }

  // Cek perintah Serial
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'r' || cmd == 'R') {
      trigger = true;
    }
    // Buang sisa karakter di buffer Serial
    while (Serial.available()) Serial.read();
  }

  if (trigger && !recording) {
    recordAudio();
  }
}

void recordAudio() {
  recording = true;

  // Nyalakan LED sebagai tanda sedang merekam
  digitalWrite(LED_PIN, HIGH);

  // Kirim penanda awal ke Serial (agar skrip Python tahu rekaman dimulai)
  Serial.println("<<REC_START>>");

  // Bersihkan buffer DMA
  i2s_zero_dma_buffer(I2S_PORT);
  delay(50);

  // ========== PROSES PEREKAMAN 1 DETIK ==========
  int samples_recorded = 0;
  int32_t temp_buffer[DMA_BUF_LEN];

  while (samples_recorded < TOTAL_SAMPLES) {
    size_t bytes_read = 0;
    int samples_to_read = min((int)DMA_BUF_LEN, TOTAL_SAMPLES - samples_recorded);

    i2s_read(I2S_PORT, temp_buffer, samples_to_read * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    int samples_got = bytes_read / sizeof(int32_t);

    for (int i = 0; i < samples_got && samples_recorded < TOTAL_SAMPLES; i++) {
      // Konversi dari 32-bit (INMP441) ke 16-bit PCM
      // INMP441 mengirim data 24-bit di container 32-bit (MSB-aligned)
      // Geser kanan 16 bit untuk mendapatkan 16-bit teratas
      audio_buffer[samples_recorded] = (int16_t)(temp_buffer[i] >> 16);
      samples_recorded++;
    }
  }

  // Matikan LED (rekaman selesai)
  digitalWrite(LED_PIN, LOW);

  // ========== KIRIM DATA KE LAPTOP VIA SERIAL ==========
  // Kirim header: jumlah sampel dan sampling rate
  Serial.printf("<<AUDIO_DATA samples=%d rate=%d>>\n", TOTAL_SAMPLES, SAMPLE_RATE);

  // Kirim data audio sebagai byte mentah (binary)
  Serial.write((uint8_t*)audio_buffer, TOTAL_SAMPLES * sizeof(int16_t));

  // Kirim penanda akhir
  Serial.println("\n<<REC_END>>");

  Serial.printf(">> Rekaman selesai! (%d sampel, %d Hz, durasi %d detik)\n",
                TOTAL_SAMPLES, SAMPLE_RATE, RECORD_SECONDS);
  Serial.println(">> Tekan tombol BOOT lagi untuk merekam ulang.");

  recording = false;
}
