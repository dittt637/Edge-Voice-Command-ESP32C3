"""
============================================================
 PEREKAM DATASET AUDIO WAV (DARI STREAMING ESP32-C3)
============================================================
 Proyek  : Edge Voice Command Recognition - Instrumentasi Cerdas UGM
 Board   : ESP32-C3 SuperMini + INMP441 (Streaming PCM 16-bit)
 
 FUNGSI  : Menangkap audio 1 detik dari ESP32-C3 dan otomatis
           menyimpannya sebagai file WAV mono 16 kHz yang siap
           di-drag & drop langsung ke Edge Impulse.

 CARA PAKAI:
   1. Pastikan ESP32 sudah terhubung di COM3 (kode streaming PCM aktif)
   2. Tutup Arduino Serial Monitor / plesp.py agar COM3 tidak bentrok
   3. Jalankan: python rekam_dataset_wav.py
   4. Masukkan label kata (misal: maju)
   5. Tekan [ENTER] -> ucapkan kata -> tersimpan sebagai .wav!
============================================================
"""

import serial
import numpy as np
import wave
import os
import time
import sys

# ==================== KONFIGURASI ====================
PORT = "COM3"           # Port COM ESP32-C3 kamu
BAUD = 115200
FS = 16000              # 16 kHz
BLOCK = 128             # Sesuai BLOCK_SIZE di kode ESP32
OUTPUT_DIR = "dataset_wav"
RECORD_SECONDS = 1.0    # 1.0 detik = 16.000 titik sampel
# =====================================================


def read_exact_bytes(ser, num_bytes, timeout_sec=3.0):
    """Membaca tepat sejumlah byte dari Serial."""
    data = bytearray()
    start = time.time()
    while len(data) < num_bytes:
        chunk = ser.read(num_bytes - len(data))
        if chunk:
            data.extend(chunk)
            start = time.time()
        elif time.time() - start > timeout_sec:
            raise TimeoutError("Tidak ada data dari ESP32. Pastikan firmware streaming berjalan.")
    return bytes(data)


def save_to_wav(filepath, pcm_samples, sample_rate=16000):
    """Menyimpan sampel int16 ke file WAV mono 16-bit."""
    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(1)                  # Mono
        wf.setsampwidth(2)                  # 16-bit (2 bytes)
        wf.setframerate(sample_rate)        # 16000 Hz
        wf.writeframes(pcm_samples.tobytes())


def record_sample(ser, seconds=1.0, sample_rate=16000):
    """Merekam tepat 1 detik audio dari stream Serial."""
    total_samples = int(seconds * sample_rate)
    samples_collected = []
    
    # Bersihkan buffer sisa agar merekam suara saat ini
    ser.reset_input_buffer()
    
    print(f"  🎙️ [RECORDING 1 DETIK...] Ucapkan sekarang!")
    start_t = time.time()
    
    while len(samples_collected) < total_samples:
        raw_bytes = read_exact_bytes(ser, BLOCK * 2)
        pcm_chunk = np.frombuffer(raw_bytes, dtype="<i2")
        samples_collected.extend(pcm_chunk)
        
    pcm_array = np.array(samples_collected[:total_samples], dtype=np.int16)
    
    # Hitung RMS energi suara untuk memverifikasi suara masuk
    rms = np.sqrt(np.mean((pcm_array.astype(np.float32) / 32768.0) ** 2))
    peak = np.max(np.abs(pcm_array))
    
    status = "JELAS" if rms > 0.02 else ("LEMAH / HENING" if rms < 0.005 else "CUKUP")
    print(f"  ✅ Selesai! (RMS: {rms:.4f} | Peak: {peak} | Suara: {status})")
    
    return pcm_array


def main():
    print("=" * 60)
    print("   PEREKAM DATASET WAV (ESP32-C3 + INMP441)")
    print("   Siap Upload ke Edge Impulse - FMIPA UGM")
    print("=" * 60)
    print()
    
    print(f"Menghubungkan ke {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1.0)
        time.sleep(1.5)
        ser.reset_input_buffer()
        print(f">> BERHASIL TERHUBUNG KE {PORT}!\n")
    except Exception as e:
        print(f">> GAGAL terhubung ke {PORT}: {e}")
        print("   Pastikan Serial Monitor Arduino / plesp.py sudah DITUTUP.")
        sys.exit(1)
        
    valid_labels = ["maju", "mundur", "kanan", "kiri", "noise"]
    print("Pilihan label yang tersedia:")
    for i, lbl in enumerate(valid_labels, 1):
        print(f"  {i}. {lbl}")
    print()
    
    label = input("Pilih label kata yang ingin kamu rekam (misal: maju): ").strip().lower()
    if label not in valid_labels:
        print(f"Label '{label}' baru akan dibuatkan foldernya.")
        
    target_folder = os.path.join(OUTPUT_DIR, label)
    os.makedirs(target_folder, exist_ok=True)
    
    # Hitung index file yang sudah ada
    existing = [f for f in os.listdir(target_folder) if f.endswith(".wav")]
    idx = len(existing) + 1
    
    print("\n" + "=" * 60)
    print(f"  Target Kelas : [{label.upper()}]")
    print(f"  Folder       : {target_folder}/")
    print(f"  File Tersedia: {len(existing)} file")
    print("  CARA: Tekan [ENTER] -> langsung ucapkan kata dengan jelas (1 detik)")
    print("  Ketik 'q' lalu ENTER untuk selesai / ganti label.")
    print("=" * 60 + "\n")
    
    try:
        while True:
            cmd = input(f"Tekan [ENTER] untuk rekam sampel ke-{idx} (atau 'q' untuk keluar): ")
            if cmd.strip().lower() == "q":
                break
                
            pcm_data = record_sample(ser, seconds=RECORD_SECONDS, sample_rate=FS)
            
            filename = f"{label}_{idx:03d}.wav"
            filepath = os.path.join(target_folder, filename)
            save_to_wav(filepath, pcm_data, FS)
            
            print(f"  💾 TERSIMPAN -> {filepath}\n")
            idx += 1
            
    except KeyboardInterrupt:
        print("\n\nSesi dihentikan oleh pengguna.")
        
    ser.close()
    print(">> Selesai! Port serial ditutup.")
    print(f">> Seluruh file WAV tersimpan di folder: {target_folder}")


if __name__ == "__main__":
    main()
