"""
============================================================
 SKRIP PENDAMPING: PENANGKAP DATA AUDIO DARI ESP32-C3
============================================================
 Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)

 FUNGSI  : Menangkap data audio mentah yang dikirim oleh ESP32-C3
           via Serial, lalu menyimpannya sebagai file WAV 16 kHz
           yang siap diunggah ke Edge Impulse untuk training model.

 CARA PAKAI:
   1. Upload program "2_rekam_dataset.ino" ke ESP32-C3
   2. Jalankan skrip ini: python capture_wav.py
   3. Masukkan label kelas (maju / mundur / kanan / kiri / noise)
   4. Tekan tombol BOOT di ESP32-C3
   5. Ucapkan kata saat LED menyala
   6. File WAV otomatis tersimpan di folder dataset/

 KEBUTUHAN:
   pip install pyserial

 CATATAN:
   Ganti variabel SERIAL_PORT sesuai port ESP32-C3 di laptopmu.
   Cek di Arduino IDE: Tools > Port (misal COM5, COM8, dll.)
============================================================
"""

import serial
import wave
import struct
import os
import time
import sys

# ==================== KONFIGURASI ====================
SERIAL_PORT = "COM5"        # <-- GANTI sesuai port ESP32-C3 kamu!
BAUD_RATE = 115200
SAMPLE_RATE = 16000          # Sampling rate 16 kHz
BITS_PER_SAMPLE = 16         # 16-bit PCM
NUM_CHANNELS = 1             # Mono
TOTAL_SAMPLES = 16000        # 1 detik = 16000 sampel
OUTPUT_DIR = "dataset"       # Folder penyimpanan file WAV
# =====================================================


def setup_directories(label):
    """Buat folder dataset/<label> jika belum ada."""
    folder = os.path.join(OUTPUT_DIR, label)
    os.makedirs(folder, exist_ok=True)
    return folder


def count_existing(folder):
    """Hitung jumlah file WAV yang sudah ada di folder."""
    return len([f for f in os.listdir(folder) if f.endswith(".wav")])


def save_wav(filepath, audio_data, sample_rate):
    """Simpan data audio sebagai file WAV."""
    with wave.open(filepath, "w") as wf:
        wf.setnchannels(NUM_CHANNELS)
        wf.setsampwidth(BITS_PER_SAMPLE // 8)  # 2 bytes (16-bit)
        wf.setframerate(sample_rate)
        wf.writeframes(audio_data)


def main():
    print("=" * 55)
    print("  PENANGKAP DATASET SUARA - Edge Voice Command")
    print("  Instrumentasi Cerdas - UGM")
    print("=" * 55)
    print()

    # Pilih label kelas
    print("Pilih label kelas yang akan direkam:")
    print("  1. maju")
    print("  2. mundur")
    print("  3. kanan")
    print("  4. kiri")
    print("  5. noise")
    print()

    label_input = input("Masukkan label (misal: maju): ").strip().lower()
    valid_labels = ["maju", "mundur", "kanan", "kiri", "noise"]

    if label_input not in valid_labels:
        print(f"Label '{label_input}' tidak valid! Pilih: {valid_labels}")
        sys.exit(1)

    folder = setup_directories(label_input)
    existing_count = count_existing(folder)
    print(f"\nFolder: {folder}")
    print(f"File yang sudah ada: {existing_count} file")
    print()

    # Buka koneksi Serial ke ESP32-C3
    print(f"Menghubungkan ke {SERIAL_PORT} @ {BAUD_RATE} baud...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)
        time.sleep(2)  # Tunggu ESP32 reset setelah koneksi Serial terbuka
        print(">> Terhubung ke ESP32-C3!")
    except serial.SerialException as e:
        print(f"GAGAL terhubung ke {SERIAL_PORT}: {e}")
        print("Pastikan port sudah benar dan Arduino Serial Monitor ditutup!")
        sys.exit(1)

    # Buang data sisa di buffer Serial
    ser.reset_input_buffer()

    sample_number = existing_count + 1

    print()
    print("=" * 55)
    print(f"  Mode Rekam: [{label_input.upper()}]")
    print("  Tekan tombol BOOT di ESP32-C3 untuk merekam.")
    print("  Tekan Ctrl+C untuk berhenti.")
    print("=" * 55)
    print()

    try:
        while True:
            # Baca baris demi baris dari Serial
            line = ser.readline().decode("utf-8", errors="ignore").strip()

            if line == "<<REC_START>>":
                print(f"  [{sample_number}] Merekam... Ucapkan '{label_input.upper()}'!")

            elif line.startswith("<<AUDIO_DATA"):
                # Parse header: <<AUDIO_DATA samples=16000 rate=16000>>
                try:
                    parts = line.replace("<<", "").replace(">>", "").split()
                    num_samples = int(parts[1].split("=")[1])
                    rate = int(parts[2].split("=")[1])
                except (IndexError, ValueError):
                    num_samples = TOTAL_SAMPLES
                    rate = SAMPLE_RATE

                # Baca data audio mentah (binary)
                expected_bytes = num_samples * (BITS_PER_SAMPLE // 8)
                audio_bytes = ser.read(expected_bytes)

                if len(audio_bytes) == expected_bytes:
                    # Simpan sebagai WAV
                    filename = f"{label_input}_{sample_number:03d}.wav"
                    filepath = os.path.join(folder, filename)
                    save_wav(filepath, audio_bytes, rate)
                    print(f"  [{sample_number}] TERSIMPAN -> {filepath}")
                    sample_number += 1
                else:
                    print(f"  [!] Data tidak lengkap: {len(audio_bytes)}/{expected_bytes} bytes")
                    print("      Coba tekan tombol BOOT lagi.")

            elif line == "<<REC_END>>":
                print(f"  >> Total file: {sample_number - 1} | Tekan BOOT lagi...")
                print()

            elif line and not line.startswith(">>"):
                # Tampilkan pesan debug dari ESP32
                pass

    except KeyboardInterrupt:
        print("\n\n>> Perekaman dihentikan oleh pengguna.")
        print(f">> Total file tersimpan: {sample_number - 1}")
        ser.close()


if __name__ == "__main__":
    main()
