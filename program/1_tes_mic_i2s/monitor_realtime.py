"""
============================================================
 MONITOR REAL-TIME MIKROFON INMP441 (dari ESP32-C3 ke PC)
============================================================
 Proyek  : Edge Voice Command Recognition - Instrumentasi Cerdas UGM
 
 FUNGSI  : Membaca data RMS dan Peak dari ESP32-C3 via Serial,
           lalu menampilkan grafik real-time di PC dan 
           menyimpan log ke file CSV.
 
 FORMAT DATA YANG DITERIMA DARI ESP32:
   "RMS:18627 Peak:60022"

 CARA PAKAI:
   1. Pastikan program 1_tes_mic_i2s.ino sudah berjalan di ESP32-C3
   2. TUTUP Arduino IDE Serial Monitor terlebih dahulu!
   3. Install kebutuhan: pip install pyserial matplotlib
   4. Jalankan: python monitor_realtime.py
   5. Bicara di depan mic -> grafik bergerak!
   6. Tekan Ctrl+C untuk berhenti dan menyimpan CSV

 CATATAN:
   Port COM3 sudah disesuaikan dengan hasil screenshot-mu.
   Jika port berubah, ganti variabel SERIAL_PORT di bawah.
============================================================
"""

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import csv
import time
import threading
import sys
import os

# ==================== KONFIGURASI ====================
SERIAL_PORT   = "COM3"      # <-- Sesuai screenshot-mu (COM3)
BAUD_RATE     = 115200
MAX_POINTS    = 100         # Jumlah titik data yang ditampilkan di grafik
OUTPUT_CSV    = "log_mic_inmp441.csv"  # File log otomatis tersimpan di sini
VAD_THRESHOLD = 5000        # Ambang RMS untuk deteksi suara aktif (VAD)
                             # Jika RMS di atas nilai ini, dianggap ada suara
# =====================================================


# Buffer data untuk grafik (rolling window)
rms_data  = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)
peak_data = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)
time_data = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)

# Counter dan log
sample_count = 0
log_rows = []
start_time = time.time()
running = True

# Koneksi Serial
ser = None


def connect_serial():
    """Buka koneksi Serial ke ESP32-C3."""
    global ser
    print(f"Menghubungkan ke {SERIAL_PORT} @ {BAUD_RATE} baud...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)  # Tunggu ESP32 reset
        ser.reset_input_buffer()
        print(f">> Terhubung ke ESP32-C3 pada {SERIAL_PORT}!\n")
        return True
    except serial.SerialException as e:
        print(f"\nGAGAL terhubung ke {SERIAL_PORT}!")
        print(f"Error: {e}")
        print("\nCara memperbaiki:")
        print("  1. Pastikan Arduino IDE Serial Monitor sudah DITUTUP.")
        print(f"  2. Pastikan ESP32-C3 terhubung dan port-nya benar.")
        print(f"  3. Cek di Arduino IDE: Tools > Port")
        print(f"  4. Ganti variabel SERIAL_PORT di baris 46 program ini.")
        return False


def parse_line(line):
    """
    Parse satu baris data dari ESP32.
    Format: "RMS:18627 Peak:60022"
    Return: (rms, peak) atau (None, None) jika gagal parse
    """
    try:
        line = line.strip()
        if "RMS:" not in line or "Peak:" not in line:
            return None, None
        
        parts = line.split()
        rms  = int(parts[0].split(":")[1])
        peak = int(parts[1].split(":")[1])
        return rms, peak
    except (IndexError, ValueError):
        return None, None


def read_serial_thread():
    """Thread terpisah untuk membaca Serial agar grafik tidak lag."""
    global sample_count, running
    
    while running:
        try:
            if ser and ser.in_waiting > 0:
                raw = ser.readline()
                line = raw.decode("utf-8", errors="ignore").strip()
                
                rms, peak = parse_line(line)
                
                if rms is not None:
                    elapsed = time.time() - start_time
                    rms_data.append(rms)
                    peak_data.append(peak)
                    time_data.append(elapsed)
                    sample_count += 1
                    
                    # Simpan ke log
                    log_rows.append({
                        "sample": sample_count,
                        "waktu_detik": round(elapsed, 3),
                        "rms": rms,
                        "peak": peak,
                        "suara_aktif": "YA" if rms > VAD_THRESHOLD else "tidak"
                    })
                    
        except Exception:
            pass
        
        time.sleep(0.01)


def save_csv():
    """Simpan log ke file CSV."""
    if not log_rows:
        return
    
    with open(OUTPUT_CSV, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["sample", "waktu_detik", "rms", "peak", "suara_aktif"])
        writer.writeheader()
        writer.writerows(log_rows)
    
    print(f"\n>> Log tersimpan: {OUTPUT_CSV} ({len(log_rows)} baris data)")


def main():
    global running
    
    print("=" * 55)
    print("  MONITOR REAL-TIME MIKROFON INMP441")
    print("  Instrumentasi Cerdas - UGM")
    print("=" * 55)
    print()
    
    if not connect_serial():
        sys.exit(1)
    
    print(f"  Ambang VAD (Voice Activity): RMS > {VAD_THRESHOLD:,}")
    print(f"  Jendela Grafik: {MAX_POINTS} sampel terakhir")
    print(f"  Log otomatis: {OUTPUT_CSV}")
    print()
    print("  >> Bicara di depan mikrofon -> grafik akan bergerak!")
    print("  >> Tekan Ctrl+C untuk berhenti.\n")

    # Mulai thread pembacaan Serial
    t = threading.Thread(target=read_serial_thread, daemon=True)
    t.start()
    
    # ========== SETUP GRAFIK MATPLOTLIB ==========
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7))
    fig.suptitle("Monitor Real-Time Mikrofon INMP441 | ESP32-C3\nInstrumentasi Cerdas - UGM",
                 fontsize=12, fontweight="bold")
    fig.patch.set_facecolor("#1e1e2e")
    
    for ax in [ax1, ax2]:
        ax.set_facecolor("#2a2a3e")
        ax.tick_params(colors="white")
        ax.yaxis.label.set_color("white")
        ax.xaxis.label.set_color("white")
        ax.title.set_color("white")
        for spine in ax.spines.values():
            spine.set_edgecolor("#555577")
    
    # Grafik RMS (energi suara)
    line_rms,  = ax1.plot([], [], color="#00ff88", linewidth=1.5, label="RMS (Energi Suara)")
    line_vad   = ax1.axhline(y=VAD_THRESHOLD, color="#ff4444", linewidth=1.5,
                              linestyle="--", label=f"Ambang VAD ({VAD_THRESHOLD:,})")
    ax1.set_title("Energi RMS (Voice Activity Detection / VAD)", fontsize=10)
    ax1.set_ylabel("Amplitudo RMS")
    ax1.set_ylim(0, 50000)
    ax1.legend(loc="upper right", fontsize=8, facecolor="#333355", labelcolor="white")
    ax1.grid(True, alpha=0.2, color="#555577")
    
    # Grafik Peak (nilai puncak)
    line_peak, = ax2.plot([], [], color="#ff9900", linewidth=1.5, label="Peak (Nilai Puncak)")
    ax2.set_title("Amplitudo Peak (Nilai Tertinggi per Frame)", fontsize=10)
    ax2.set_ylabel("Amplitudo Peak")
    ax2.set_xlabel("Sampel ke-n (100 sampel terakhir)")
    ax2.set_ylim(0, 150000)
    ax2.legend(loc="upper right", fontsize=8, facecolor="#333355", labelcolor="white")
    ax2.grid(True, alpha=0.2, color="#555577")
    
    # Teks status VAD
    status_text = ax1.text(0.02, 0.88, "STATUS: HENING",
                            transform=ax1.transAxes, fontsize=11,
                            fontweight="bold", color="#888888",
                            bbox=dict(boxstyle="round,pad=0.3", facecolor="#2a2a3e",
                                      edgecolor="#555577"))
    
    # Teks info sampel
    info_text = fig.text(0.02, 0.02, "Sampel: 0 | Waktu: 0.0 s",
                          fontsize=9, color="#aaaaaa")
    
    x_axis = list(range(MAX_POINTS))
    
    def update_plot(frame):
        """Update grafik setiap frame animasi."""
        rms_list  = list(rms_data)
        peak_list = list(peak_data)
        
        line_rms.set_data(x_axis, rms_list)
        line_peak.set_data(x_axis, peak_list)
        
        # Update status VAD
        current_rms = rms_list[-1] if rms_list else 0
        if current_rms > VAD_THRESHOLD:
            status_text.set_text(f"STATUS: 🎤 SUARA TERDETEKSI! (RMS: {current_rms:,})")
            status_text.set_color("#00ff88")
            status_text.get_bbox_patch().set_edgecolor("#00ff88")
        else:
            status_text.set_text(f"STATUS: Hening (RMS: {current_rms:,})")
            status_text.set_color("#888888")
            status_text.get_bbox_patch().set_edgecolor("#555577")
        
        # Update info
        elapsed = time.time() - start_time
        info_text.set_text(f"Sampel: {sample_count:,} | "
                           f"Waktu: {elapsed:.1f} detik | "
                           f"Port: {SERIAL_PORT} | "
                           f"Tekan Ctrl+C untuk berhenti")
        
        return line_rms, line_peak, status_text, info_text
    
    ani = animation.FuncAnimation(fig, update_plot, interval=50,
                                   blit=False, cache_frame_data=False)
    
    try:
        plt.tight_layout(rect=[0, 0.04, 1, 0.93])
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        running = False
        if ser and ser.is_open:
            ser.close()
        save_csv()
        print(f"\n>> Monitor dihentikan. Total sampel dibaca: {sample_count:,}")


if __name__ == "__main__":
    main()
