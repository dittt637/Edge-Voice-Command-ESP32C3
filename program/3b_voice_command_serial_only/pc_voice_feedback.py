"""
============================================================
 OPSIONAL: BUNYI KE SPEAKER LAPTOP VIA SERIAL (TANPA MAX98357A)
============================================================
 Proyek  : Edge Voice Command Recognition - Instrumentasi Cerdas UGM

 FUNGSI  : Gunakan skrip ini jika kamu BELUM memasang modul amplifier
           MAX98357A di ESP32-C3, tapi ingin laptop langsung mengucapkan
           "forward", "left", "right", atau "stop" ke speaker laptop
           begitu ESP32-C3 mendeteksi suaramu!

 CARA PAKAI:
   1. Pastikan program 3b_voice_command_serial_only.ino sudah ter-upload di ESP32-C3
   2. TUTUP Serial Monitor Arduino IDE agar COM3 tidak bentrok
   3. Jalankan: python pc_voice_feedback.py
   4. Ucapkan kata perintah ke mic INMP441 -> laptop langsung bersuara!
============================================================
"""

import serial
import sounddevice as sd
import numpy as np
import subprocess
import tempfile
import wave
import os
import re
import time
import sys

# ==================== KONFIGURASI ====================
PORT = "COM3"
BAUD = 115200
WORDS = ["forward", "left", "right", "stop"]
# =====================================================


def prepare_tts_clips():
    """Buat klip suara di memori menggunakan Windows Speech Synthesizer."""
    clips = {}
    tmp_dir = tempfile.mkdtemp(prefix="ei_tts_")

    ps_lines = [
        "Add-Type -AssemblyName System.Speech",
        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer",
        "$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(16000, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)",
    ]
    for w in WORDS:
        wav_path = os.path.join(tmp_dir, f"{w}.wav").replace("\\", "/")
        ps_lines.append(f'$s.SetOutputToWaveFile("{wav_path}", $fmt)')
        ps_lines.append(f'$s.Speak("{w}")')
    ps_lines.append("$s.SetOutputToNull(); $s.Dispose()")

    subprocess.run(
        ["powershell", "-NoProfile", "-Command", "; ".join(ps_lines)],
        check=True
    )

    for w in WORDS:
        wav_path = os.path.join(tmp_dir, f"{w}.wav")
        with wave.open(wav_path, "rb") as wf:
            raw = np.frombuffer(wf.readframes(wf.getnframes()), dtype=np.int16).astype(np.float32) / 32768.0
            nz = np.where(np.abs(raw) > 0.01)[0]
            if len(nz):
                raw = raw[max(0, nz[0] - 800):min(len(raw), nz[-1] + 800)]
            clips[w] = raw
        os.remove(wav_path)

    os.rmdir(tmp_dir)
    return clips


def main():
    print("Menyiapkan audio TTS Windows...")
    clips = prepare_tts_clips()
    print(f"Klip siap: {list(clips.keys())}")

    print(f"Menghubungkan ke {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.5)
    except serial.SerialException as e:
        print(f"GAGAL membuka {PORT}: {e}")
        print("Pastikan Serial Monitor Arduino IDE sudah DITUTUP!")
        sys.exit(1)

    print("Terhubung! Silakan ucapkan perintah ke mic INMP441...\n")

    try:
        while True:
            raw_line = ser.readline()
            if not raw_line:
                continue
            line = raw_line.decode("utf-8", errors="ignore").rstrip()
            print(line)

            m = re.search(r"TERDETEKSI:\s*\[(\w+)\]", line)
            if m:
                word = m.group(1).lower()
                if word in clips:
                    sd.play(clips[word], samplerate=16000)
                    sd.wait()
                    # Buang audio yang mungkin tertangkap selama speaker laptop berbunyi
                    ser.reset_input_buffer()
    except KeyboardInterrupt:
        print("\nDihentikan oleh pengguna.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
