"""
============================================================
 PROGRAM 4: UJI KOMPARASI EDGE AI vs CLOUD AI
============================================================
 Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)

 FUNGSI  : Mengirim file rekaman suara WAV ke Google Speech Recognition
           API (Cloud), mengukur latensi respon dan akurasi tebakan,
           lalu membandingkan hasilnya dengan performa ESP32-C3 (Edge).

           Hasil perbandingan otomatis ditampilkan dalam tabel dan
           disimpan sebagai file CSV untuk dimasukkan ke laporan.

 CARA PAKAI:
   1. Pastikan laptop terhubung ke internet
   2. Siapkan file WAV dataset di folder "dataset/" (hasil dari Program 2)
   3. Jalankan: python uji_google_cloud.py
   4. Hasil otomatis tersimpan di file "hasil_komparasi.csv"

 KEBUTUHAN:
   pip install SpeechRecognition

 CATATAN:
   Google Speech Recognition API gratis ini memiliki batasan jumlah request.
   Jangan mengirim lebih dari ~50 request per menit agar tidak diblokir.
============================================================
"""

import speech_recognition as sr
import os
import time
import csv
import sys
from pathlib import Path

# ==================== KONFIGURASI ====================
DATASET_DIR = "dataset"           # Folder berisi subfolder: maju/, mundur/, kanan/, kiri/, noise/
OUTPUT_CSV = "hasil_komparasi.csv" # File output hasil pengujian
LANGUAGE = "id-ID"                 # Bahasa Indonesia
MAX_FILES_PER_CLASS = 20           # Maksimal file yang diuji per kelas
DELAY_BETWEEN_REQUESTS = 1.5      # Jeda antar request (detik) agar tidak kena rate limit
# =====================================================


def test_single_file(recognizer, filepath, expected_label):
    """
    Kirim 1 file WAV ke Google Cloud Speech API dan ukur hasilnya.

    Return:
      dict: {
        'file': nama file,
        'expected': label sebenarnya,
        'predicted': hasil tebakan Google,
        'correct': True/False,
        'latency_ms': waktu respon dalam milidetik,
        'error': pesan error jika gagal
      }
    """
    result = {
        "file": os.path.basename(filepath),
        "expected": expected_label,
        "predicted": "",
        "correct": False,
        "latency_ms": 0,
        "error": ""
    }

    try:
        with sr.AudioFile(filepath) as source:
            audio = recognizer.record(source)

        # Ukur waktu mulai
        start_time = time.time()

        # Kirim ke Google Speech Recognition API
        text = recognizer.recognize_google(audio, language=LANGUAGE)

        # Ukur waktu selesai
        end_time = time.time()
        latency = (end_time - start_time) * 1000  # Konversi ke milidetik

        # Normalisasi hasil tebakan (lowercase, hilangkan spasi ekstra)
        predicted = text.strip().lower()

        result["predicted"] = predicted
        result["latency_ms"] = round(latency, 1)

        # Cek apakah tebakan mengandung kata yang diharapkan
        # (Google sering menambahkan kata lain, misal "maju" jadi "maju terus")
        if expected_label.lower() in predicted:
            result["correct"] = True

    except sr.UnknownValueError:
        result["predicted"] = "(tidak terdeteksi)"
        result["error"] = "Google tidak bisa mengenali suara"

    except sr.RequestError as e:
        result["predicted"] = "(error)"
        result["error"] = f"Gagal terhubung ke Google API: {e}"

    except Exception as e:
        result["predicted"] = "(error)"
        result["error"] = str(e)

    return result


def main():
    print("=" * 60)
    print("  UJI KOMPARASI: EDGE AI (ESP32-C3) vs CLOUD AI (Google)")
    print("  Instrumentasi Cerdas - UGM")
    print("=" * 60)
    print()

    # Cek apakah folder dataset ada
    if not os.path.exists(DATASET_DIR):
        print(f"FOLDER '{DATASET_DIR}' TIDAK DITEMUKAN!")
        print("Pastikan kamu sudah merekam dataset menggunakan Program 2.")
        print(f"Struktur folder yang diharapkan:")
        print(f"  {DATASET_DIR}/")
        print(f"    maju/   -> maju_001.wav, maju_002.wav, ...")
        print(f"    mundur/ -> mundur_001.wav, mundur_002.wav, ...")
        print(f"    kanan/  -> kanan_001.wav, kanan_002.wav, ...")
        print(f"    kiri/   -> kiri_001.wav, kiri_002.wav, ...")
        print(f"    noise/  -> noise_001.wav, noise_002.wav, ...")
        sys.exit(1)

    # Daftar kelas yang akan diuji
    classes = ["maju", "mundur", "kanan", "kiri", "noise"]
    recognizer = sr.Recognizer()

    all_results = []
    class_stats = {}

    print(f"Bahasa pengenalan: {LANGUAGE}")
    print(f"Folder dataset: {DATASET_DIR}")
    print(f"Maks file per kelas: {MAX_FILES_PER_CLASS}")
    print()

    for label in classes:
        folder = os.path.join(DATASET_DIR, label)

        if not os.path.exists(folder):
            print(f"  [SKIP] Folder '{folder}' tidak ditemukan.")
            continue

        # Ambil daftar file WAV
        wav_files = sorted([f for f in os.listdir(folder) if f.endswith(".wav")])
        wav_files = wav_files[:MAX_FILES_PER_CLASS]

        if len(wav_files) == 0:
            print(f"  [SKIP] Tidak ada file WAV di '{folder}'.")
            continue

        print(f"  Menguji kelas [{label.upper()}]: {len(wav_files)} file...")

        correct_count = 0
        total_latency = 0

        for i, filename in enumerate(wav_files):
            filepath = os.path.join(folder, filename)
            result = test_single_file(recognizer, filepath, label)
            all_results.append(result)

            status = "OK" if result["correct"] else "SALAH"
            if result["error"]:
                status = "ERROR"

            print(f"    [{i+1}/{len(wav_files)}] {filename}: "
                  f"Google bilang \"{result['predicted']}\" -> {status} "
                  f"({result['latency_ms']} ms)")

            if result["correct"]:
                correct_count += 1
            total_latency += result["latency_ms"]

            # Jeda antar request
            time.sleep(DELAY_BETWEEN_REQUESTS)

        # Statistik per kelas
        accuracy = (correct_count / len(wav_files) * 100) if wav_files else 0
        avg_latency = (total_latency / len(wav_files)) if wav_files else 0

        class_stats[label] = {
            "total": len(wav_files),
            "correct": correct_count,
            "accuracy": accuracy,
            "avg_latency_ms": round(avg_latency, 1)
        }

        print(f"    >> Akurasi: {correct_count}/{len(wav_files)} "
              f"({accuracy:.1f}%) | Rata-rata latensi: {avg_latency:.0f} ms")
        print()

    # ========== TAMPILKAN RINGKASAN ==========
    print()
    print("=" * 60)
    print("  RINGKASAN HASIL KOMPARASI")
    print("=" * 60)
    print()

    # Header tabel
    print(f"{'Kelas':<10} {'Benar':<10} {'Total':<10} {'Akurasi':<12} {'Latensi (ms)':<15}")
    print("-" * 57)

    total_correct = 0
    total_files = 0
    total_lat = 0

    for label in classes:
        if label in class_stats:
            s = class_stats[label]
            print(f"{label.upper():<10} {s['correct']:<10} {s['total']:<10} "
                  f"{s['accuracy']:.1f}%{'':<7} {s['avg_latency_ms']:<15}")
            total_correct += s["correct"]
            total_files += s["total"]
            total_lat += s["avg_latency_ms"] * s["total"]

    if total_files > 0:
        overall_accuracy = total_correct / total_files * 100
        overall_latency = total_lat / total_files
        print("-" * 57)
        print(f"{'TOTAL':<10} {total_correct:<10} {total_files:<10} "
              f"{overall_accuracy:.1f}%{'':<7} {overall_latency:.0f}")

    # ========== TABEL PERBANDINGAN EDGE vs CLOUD ==========
    print()
    print("=" * 60)
    print("  PERBANDINGAN EDGE AI vs CLOUD AI")
    print("=" * 60)
    print()
    print(f"{'Parameter':<30} {'Edge (ESP32-C3)':<20} {'Cloud (Google)':<20}")
    print("-" * 70)
    print(f"{'Latensi Rata-rata':<30} {'~85 ms':<20} {f'~{overall_latency:.0f} ms' if total_files > 0 else 'N/A':<20}")
    print(f"{'Ketergantungan Internet':<30} {'Tidak (100% Offline)':<20} {'Ya (Wajib Online)':<20}")
    print(f"{'Konsumsi RAM':<30} {'~32 KB':<20} {'Di server Google':<20}")
    print(f"{'Akurasi':<30} {'(dari Edge Impulse)':<20} {f'{overall_accuracy:.1f}%' if total_files > 0 else 'N/A':<20}")
    print(f"{'Biaya':<30} {'Gratis (on-device)':<20} {'Gratis (terbatas)':<20}")
    print()

    # ========== SIMPAN KE CSV ==========
    if all_results:
        with open(OUTPUT_CSV, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=["file", "expected", "predicted",
                                                    "correct", "latency_ms", "error"])
            writer.writeheader()
            writer.writerows(all_results)

        print(f">> Hasil detail tersimpan di: {OUTPUT_CSV}")
        print(">> Gunakan data ini untuk tabel evaluasi di laporan akhir!")
    else:
        print(">> Tidak ada hasil untuk disimpan.")


if __name__ == "__main__":
    main()
