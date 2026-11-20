"""
============================================================
 PROGRAM PEREKAM SUARA KE FILE CSV (dari ESP32-C3)
============================================================
 Proyek  : Edge Voice Command Recognition - Instrumentasi Cerdas UGM
 Board   : ESP32-C3 SuperMini + Mikrofon INMP441
 
 FUNGSI  : Merekam aliran data audio PCM 16-bit dari ESP32-C3 via COM3,
           lalu menyimpannya langsung ke file CSV (Comma-Separated Values).
           
 DUA MODE PEREKAMAN:
   1. Mode Kata Perintah (1 Detik):
      Merekam 16.000 titik sampel per kata perintah ("maju", "mundur", dll.)
      dan menyimpannya ke folder dataset_csv/<label>/<label>_001.csv
      
   2. Mode Kontinu:
      Merekam secara terus-menerus selama durasi yang kamu tentukan
      (misal 5 atau 10 detik) ke dalam satu file CSV.

 FORMAT DATA DI DALAM CSV:
   timestamp_ms, sample_index, raw_pcm16, normalized
   0.000, 0, 142, 0.00433
   0.063, 1, 310, 0.00946
   ...
   (Dapat langsung dibuka di Microsoft Excel, Google Sheets, atau Pandas)

 CARA PAKAI:
   1. Pastikan ESP32 sudah terhubung di COM3 dan mengirim data audio
   2. Tutup aplikasi lain yang memakai COM3 (Arduino IDE Serial Monitor/plesp.py)
   3. Jalankan: python rekam_audio_csv.py
============================================================
"""

import serial
import numpy as np
import csv
import os
import time
import sys

# ==================== KONFIGURASI ====================
PORT = "COM3"           # Port ESP32-C3 kamu
BAUD = 115200
FS = 16000              # Sampling rate 16 kHz
BLOCK = 128             # Ukuran blok data dari ESP32
OUTPUT_DIR = "dataset_csv"
# =====================================================


def read_exact_bytes(ser, num_bytes, timeout_sec=3.0):
    """Membaca tepat sejumlah byte biner dari Serial."""
    data = bytearray()
    start = time.time()
    while len(data) < num_bytes:
        chunk = ser.read(num_bytes - len(data))
        if chunk:
            data.extend(chunk)
            start = time.time()
        elif time.time() - start > timeout_sec:
            raise TimeoutError("Tidak ada data dari ESP32 selama 3 detik. Pastikan ESP32 aktif.")
    return bytes(data)


def save_to_csv(filepath, pcm_samples, sample_rate=16000):
    """
    Menyimpan array sampel audio ke file CSV.
    Kolom: timestamp_ms, sample_index, raw_pcm16, normalized
    """
    total = len(pcm_samples)
    norm_samples = pcm_samples.astype(np.float32) / 32768.0
    
    with open(filepath, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        # Header CSV
        writer.writerow(["timestamp_ms", "sample_index", "raw_pcm16", "normalized"])
        
        # Tulis setiap sampel
        for i in range(total):
            t_ms = (i / sample_rate) * 1000.0  # waktu dalam milidetik
            writer.writerow([
                f"{t_ms:.4f}",
                i,
                int(pcm_samples[i]),
                f"{norm_samples[i]:.6f}"
            ])


def record_duration(ser, seconds, sample_rate=16000):
    """Merekam audio selama N detik dari stream Serial."""
    total_samples = int(seconds * sample_rate)
    samples_collected = []
    
    # Bersihkan buffer serial sebelum mulai
    ser.reset_input_buffer()
    
    print(f"  [●] MEREKAM SELAMA {seconds} DETIK... SILAKAN BICARA!")
    start_t = time.time()
    
    while len(samples_collected) < total_samples:
        raw_bytes = read_exact_bytes(ser, BLOCK * 2)
        pcm_chunk = np.frombuffer(raw_bytes, dtype="<i2")
        samples_collected.extend(pcm_chunk)
    
    elapsed = time.time() - start_t
    pcm_array = np.array(samples_collected[:total_samples], dtype=np.int16)
    
    # Hitung RMS energi suara rekaman
    rms = np.sqrt(np.mean((pcm_array.astype(np.float32) / 32768.0) ** 2))
    print(f"  [✓] Rekaman selesai! ({len(pcm_array)} titik data dalam {elapsed:.2f} detik | RMS: {rms:.4f})")
    
    return pcm_array


def main():
    print("=" * 60)
    print("   PEREKAM AUDIO KE CSV (INMP441 + ESP32-C3)")
    print("   Instrumentasi Cerdas - FMIPA UGM")
    print("=" * 60)
    print()
    
    # Hubungkan ke port Serial
    print(f"Menghubungkan ke {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1.0)
        time.sleep(1.5)  # Tunggu koneksi stabil
        ser.reset_input_buffer()
        print(f">> BERHASIL TERHUBUNG KE {PORT}!\n")
    except Exception as e:
        print(f">> GAGAL terhubung ke {PORT}: {e}")
        print("   Pastikan program 'plesp.py' atau Serial Monitor Arduino sudah ditutup.")
        sys.exit(1)
        
    print("PILIH MODE PEREKAMAN:")
    print("  1. Mode Dataset Kata (1 detik per kata: maju, mundur, kanan, kiri, noise)")
    print("  2. Mode Rekam Kontinu (tentukan sendiri berapa detik)")
    print()
    
    mode = input("Pilih mode (1 atau 2): ").strip()
    
    if mode == "1":
        # MODE 1: DATASET 1 DETIK PER KATA
        valid_labels = ["maju", "mundur", "kanan", "kiri", "noise"]
        print(f"\nLabel yang tersedia: {', '.join(valid_labels)}")
        label = input("Masukkan label kata yang mau direkam: ").strip().lower()
        
        if label not in valid_labels:
            print(f"Label '{label}' tidak dikenal, tapi tetap akan dibuatkan foldernya.")
            
        target_folder = os.path.join(OUTPUT_DIR, label)
        os.makedirs(target_folder, exist_ok=True)
        
        # Hitung nomor file berikutnya
        existing = [f for f in os.listdir(target_folder) if f.endswith(".csv")]
        idx = len(existing) + 1
        
        print("\n" + "=" * 60)
        print(f"  Mode Aktif : [{label.upper()}]")
        print(f"  Folder     : {target_folder}/")
        print("  Setiap tekan ENTER -> sistem akan merekam 1 detik (16.000 titik data)")
        print("  Ketik 'q' lalu ENTER untuk keluar.")
        print("=" * 60 + "\n")
        
        while True:
            cmd = input(f"Tekan [ENTER] untuk merekam sampel ke-{idx} (atau 'q' untuk selesai): ")
            if cmd.strip().lower() == "q":
                break
                
            pcm_data = record_duration(ser, seconds=1.0, sample_rate=FS)
            
            filename = f"{label}_{idx:03d}.csv"
            filepath = os.path.join(target_folder, filename)
            save_to_csv(filepath, pcm_data, FS)
            
            print(f"  >> FILE TERSIMPAN -> {filepath}\n")
            idx += 1
            
    else:
        # MODE 2: REKAM KONTINU
        try:
            durasi = float(input("Berapa detik rekaman yang diinginkan (misal: 5 atau 10)? ").strip())
        except ValueError:
            durasi = 5.0
            
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        nama_file = input("Beri nama file CSV (misal: tes_suara_kontinu): ").strip()
        if not nama_file.endswith(".csv"):
            nama_file += ".csv"
            
        filepath = os.path.join(OUTPUT_DIR, nama_file)
        
        input("\nTekan [ENTER] jika sudah siap mulai merekam...")
        pcm_data = record_duration(ser, seconds=durasi, sample_rate=FS)
        
        print(f"Menyimpan ke CSV...")
        save_to_csv(filepath, pcm_data, FS)
        print(f">> SUKSES TERSIMPAN KE: {filepath}\n")
        
    ser.close()
    print(">> Selesai! Port serial ditutup.")


if __name__ == "__main__":
    main()
