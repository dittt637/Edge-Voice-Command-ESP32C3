# 📂 Struktur Program Proyek
# Edge Voice Command Recognition - ESP32-C3 & TinyML
# Instrumentasi Cerdas (PAEI262502) - UGM

Folder ini berisi seluruh kode program yang dibutuhkan untuk proyek
dari awal (tes hardware) sampai akhir (evaluasi dan laporan).

## Urutan Pengerjaan

### Program 1: Tes Mikrofon (`1_tes_mic_i2s/`)
- File: `1_tes_mic_i2s.ino`
- Platform: Arduino IDE
- Fungsi: Memastikan mikrofon INMP441 dan ESP32-C3 tersambung dengan benar.
- Cara Pakai:
  1. Buka di Arduino IDE
  2. Pilih Board: "ESP32C3 Dev Module"
  3. Upload ke ESP32-C3
  4. Buka Serial Plotter -> bicara di depan mic -> grafik bergerak

### Program 2: Perekam Dataset (`2_rekam_dataset/`)
- File Arduino: `2_rekam_dataset.ino`
- File Python: `capture_wav.py`
- Fungsi: Merekam suara perintah dan menyimpan sebagai file WAV 16 kHz.
- Cara Pakai:
  1. Upload .ino ke ESP32-C3
  2. Di laptop, jalankan: `pip install pyserial`
  3. Jalankan: `python capture_wav.py`
  4. Pilih label (maju/mundur/kanan/kiri/noise)
  5. Tekan tombol BOOT di ESP32 -> ucapkan kata -> file tersimpan otomatis
- Target: 650 file WAV total (130 per kelas x 5 kelas)

### Program 3: Robot Voice Command (`3_robot_voice_command/` & `3b_voice_command_serial_only/`)
- File 3 (Dengan Indikator LED/Motor): `3_robot_voice_command/3_robot_voice_command.ino`
- File 3B (Serial Monitor Only, Tanpa LED): `3b_voice_command_serial_only/3b_voice_command_serial_only.ino`
- Platform: Arduino IDE + Library Edge Impulse (`ei-instrumentasi-voice-command-arduino-1.0.7-impulse-#1.zip`)
- Fungsi: Program utama pengenalan kata (`forward`, `left`, `right`, `stop`, `silent`).
- Prasyarat: Install library ZIP dari menu `Sketch > Include Library > Add .ZIP Library...`.

### Program 4: Pengujian & Evaluasi (`4_uji_google_cloud/`)
- File Python 1: `uji_google_cloud.py` -> Komparasi latensi Edge vs Cloud
- File Python 2: `evaluasi_model.py` -> Confusion Matrix & metrik (Akurasi,
  Precision, Recall, F1-Score)
- Kebutuhan: `pip install SpeechRecognition matplotlib numpy scikit-learn`
- Output: `confusion_matrix.png` dan `metrik_evaluasi.csv` untuk laporan akhir.

## Wiring ESP32-C3 SuperMini <-> INMP441

| INMP441 Pin | ESP32-C3 Pin | Fungsi                    |
|-------------|--------------|---------------------------|
| VDD         | 3.3V         | Catu daya                 |
| GND         | GND          | Ground                    |
| SD          | GPIO 4       | Serial Data (audio)       |
| WS          | GPIO 2       | Word Select (channel L/R) |
| SCK         | GPIO 3       | Serial Clock              |
| L/R         | GND          | Pilih channel kiri (mono) |
