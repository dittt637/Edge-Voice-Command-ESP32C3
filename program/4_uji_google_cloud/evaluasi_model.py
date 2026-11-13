"""
============================================================
 PROGRAM 5: ANALISIS EVALUASI - CONFUSION MATRIX & METRIK
============================================================
 Proyek  : Edge Voice Command Recognition (Instrumentasi Cerdas UGM)

 FUNGSI  : Membuat Confusion Matrix 5x5, menghitung Akurasi,
           Precision, Recall, F1-Score per kelas, dan
           menyimpan grafik sebagai gambar PNG untuk laporan.

 CARA PAKAI:
   1. Setelah pengujian 100 sampel pada ESP32-C3, catat hasilnya
      di file CSV "hasil_uji_edge.csv" (format: expected,predicted)
   2. Jalankan: python evaluasi_model.py
   3. Grafik Confusion Matrix tersimpan sebagai "confusion_matrix.png"
   4. Tabel metrik tersimpan sebagai "metrik_evaluasi.csv"

 KEBUTUHAN:
   pip install matplotlib numpy scikit-learn

 CATATAN:
   Jika belum punya data pengujian nyata, program akan menjalankan
   contoh data simulasi agar kamu bisa melihat format outputnya.
============================================================
"""

import numpy as np
import csv
import os
import sys

# Cek apakah library yang dibutuhkan sudah terinstall
try:
    import matplotlib
    matplotlib.use('Agg')  # Backend non-GUI agar bisa jalan tanpa display
    import matplotlib.pyplot as plt
    from matplotlib.colors import LinearSegmentedColormap
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("[INFO] matplotlib belum terinstall. Grafik tidak akan dibuat.")
    print("       Install dengan: pip install matplotlib")

try:
    from sklearn.metrics import (confusion_matrix, classification_report,
                                  accuracy_score, precision_recall_fscore_support)
    HAS_SKLEARN = True
except ImportError:
    HAS_SKLEARN = False
    print("[INFO] scikit-learn belum terinstall.")
    print("       Install dengan: pip install scikit-learn")


# ==================== KONFIGURASI ====================
INPUT_CSV = "hasil_uji_edge.csv"        # File hasil pengujian dari ESP32-C3
OUTPUT_IMAGE = "confusion_matrix.png"    # Gambar Confusion Matrix
OUTPUT_CSV = "metrik_evaluasi.csv"       # Tabel metrik per kelas
LABELS = ["Maju", "Mundur", "Kanan", "Kiri", "Noise"]
# =====================================================


def load_test_results(filepath):
    """Muat hasil pengujian dari file CSV."""
    y_true = []
    y_pred = []

    with open(filepath, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            y_true.append(row["expected"].strip().capitalize())
            y_pred.append(row["predicted"].strip().capitalize())

    return y_true, y_pred


def generate_sample_data():
    """
    Buat data simulasi untuk contoh output.
    Gunakan ini jika belum punya data pengujian nyata.
    """
    print("=" * 55)
    print("  [MODE SIMULASI] Menggunakan data contoh")
    print("  Ganti dengan data asli dari pengujian ESP32-C3")
    print("=" * 55)
    print()

    np.random.seed(42)

    y_true = []
    y_pred = []

    # Simulasi 100 sampel (20 per kelas)
    # Dengan akurasi sekitar 90%
    for label in LABELS:
        for _ in range(20):
            y_true.append(label)

            # 90% kemungkinan tebakan benar
            if np.random.random() < 0.90:
                y_pred.append(label)
            else:
                # Salah tebak -> pilih label lain secara acak
                wrong_labels = [l for l in LABELS if l != label]
                y_pred.append(np.random.choice(wrong_labels))

    return y_true, y_pred


def compute_metrics(y_true, y_pred, labels):
    """Hitung metrik evaluasi per kelas."""
    metrics = []

    for label in labels:
        # Hitung TP, FP, FN, TN untuk kelas ini
        tp = sum(1 for t, p in zip(y_true, y_pred) if t == label and p == label)
        fp = sum(1 for t, p in zip(y_true, y_pred) if t != label and p == label)
        fn = sum(1 for t, p in zip(y_true, y_pred) if t == label and p != label)
        tn = sum(1 for t, p in zip(y_true, y_pred) if t != label and p != label)

        # Hitung Precision, Recall, F1-Score
        precision = tp / (tp + fp) if (tp + fp) > 0 else 0
        recall = tp / (tp + fn) if (tp + fn) > 0 else 0
        f1 = (2 * precision * recall / (precision + recall)) if (precision + recall) > 0 else 0

        metrics.append({
            "Kelas": label,
            "TP": tp,
            "FP": fp,
            "FN": fn,
            "TN": tn,
            "Precision": round(precision * 100, 1),
            "Recall": round(recall * 100, 1),
            "F1-Score": round(f1 * 100, 1)
        })

    return metrics


def plot_confusion_matrix(y_true, y_pred, labels, output_path):
    """Buat dan simpan grafik Confusion Matrix."""
    if not HAS_MATPLOTLIB:
        print("[SKIP] Tidak bisa membuat grafik tanpa matplotlib.")
        return

    # Hitung confusion matrix
    if HAS_SKLEARN:
        cm = confusion_matrix(y_true, y_pred, labels=labels)
    else:
        # Manual computation
        n = len(labels)
        cm = np.zeros((n, n), dtype=int)
        label_to_idx = {l: i for i, l in enumerate(labels)}
        for t, p in zip(y_true, y_pred):
            if t in label_to_idx and p in label_to_idx:
                cm[label_to_idx[t]][label_to_idx[p]] += 1

    # Buat figure
    fig, ax = plt.subplots(figsize=(8, 6))

    # Warna: biru muda ke biru tua
    cmap = plt.cm.Blues

    im = ax.imshow(cm, interpolation='nearest', cmap=cmap)
    ax.figure.colorbar(im, ax=ax, shrink=0.8)

    # Label sumbu
    ax.set(xticks=np.arange(cm.shape[1]),
           yticks=np.arange(cm.shape[0]),
           xticklabels=labels,
           yticklabels=labels,
           ylabel='Label Sebenarnya (Aktual)',
           xlabel='Label Tebakan AI (Prediksi)')

    ax.set_title('Confusion Matrix - Edge Voice Command\n'
                 'Pengujian pada ESP32-C3 (Subject-Wise Test Set)',
                 fontsize=12, fontweight='bold', pad=15)

    # Rotasi label sumbu X
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")

    # Tampilkan angka di setiap sel
    thresh = cm.max() / 2.0
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            ax.text(j, i, format(cm[i, j], 'd'),
                    ha="center", va="center",
                    color="white" if cm[i, j] > thresh else "black",
                    fontsize=14, fontweight='bold')

    fig.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f">> Grafik Confusion Matrix tersimpan: {output_path}")


def main():
    print("=" * 55)
    print("  EVALUASI MODEL - Confusion Matrix & Metrik")
    print("  Instrumentasi Cerdas - UGM")
    print("=" * 55)
    print()

    # Muat data pengujian
    if os.path.exists(INPUT_CSV):
        print(f"Memuat data dari: {INPUT_CSV}")
        y_true, y_pred = load_test_results(INPUT_CSV)
        print(f"Total sampel: {len(y_true)}")
    else:
        print(f"File '{INPUT_CSV}' tidak ditemukan.")
        print("Menggunakan data simulasi sebagai contoh...")
        print()
        y_true, y_pred = generate_sample_data()

    print()

    # ========== HITUNG METRIK ==========
    metrics = compute_metrics(y_true, y_pred, LABELS)

    # Akurasi keseluruhan
    correct = sum(1 for t, p in zip(y_true, y_pred) if t == p)
    total = len(y_true)
    accuracy = correct / total * 100 if total > 0 else 0

    # ========== TAMPILKAN TABEL METRIK ==========
    print("=" * 70)
    print("  METRIK EVALUASI PER KELAS")
    print("=" * 70)
    print(f"{'Kelas':<10} {'TP':<6} {'FP':<6} {'FN':<6} {'TN':<6} "
          f"{'Precision':<12} {'Recall':<12} {'F1-Score':<12}")
    print("-" * 70)

    for m in metrics:
        print(f"{m['Kelas']:<10} {m['TP']:<6} {m['FP']:<6} {m['FN']:<6} {m['TN']:<6} "
              f"{m['Precision']:.1f}%{'':<7} {m['Recall']:.1f}%{'':<7} {m['F1-Score']:.1f}%")

    print("-" * 70)
    print(f"{'AKURASI KESELURUHAN:':<40} {accuracy:.1f}% ({correct}/{total})")
    print()

    # Cek target minimal
    target = 85.0
    if accuracy >= target:
        print(f"  >> STATUS: LULUS (Akurasi {accuracy:.1f}% >= target {target:.0f}%)")
    else:
        print(f"  >> STATUS: BELUM LULUS (Akurasi {accuracy:.1f}% < target {target:.0f}%)")
        print(f"     Perlu perbaikan model atau penambahan data training.")
    print()

    # ========== BUAT CONFUSION MATRIX ==========
    plot_confusion_matrix(y_true, y_pred, LABELS, OUTPUT_IMAGE)

    # ========== SIMPAN METRIK KE CSV ==========
    with open(OUTPUT_CSV, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Kelas", "TP", "FP", "FN", "TN",
                                                "Precision", "Recall", "F1-Score"])
        writer.writeheader()
        writer.writerows(metrics)

        # Tambah baris akurasi keseluruhan
        writer.writerow({
            "Kelas": "TOTAL",
            "TP": correct,
            "FP": "-",
            "FN": "-",
            "TN": "-",
            "Precision": "-",
            "Recall": "-",
            "F1-Score": f"{accuracy:.1f}"
        })

    print(f">> Tabel metrik tersimpan: {OUTPUT_CSV}")
    print()
    print(">> Masukkan grafik dan tabel ini ke Bab 7 (Evaluasi) laporan akhir!")


if __name__ == "__main__":
    main()
