"""museStopWatch: downloads the data microWakeWord's training notebook uses
(basic_training_notebook.ipynb, steps 4 and 8), into the current directory:

  mit_rirs/            room impulse responses (davidscripka/MIT_environmental_impulse_responses)
  audioset_16k/        AudioSet bal_train09 as 16 kHz wav (background noise)
  fma_16k/             Free Music Archive xsmall as 16 kHz wav (background music)
  negative_datasets/   kahrendt/microwakeword: speech, dinner_party, no_speech, dinner_party_eval
                       (ready-made features of sounds that are not the wake word)

Some of the augmentation data has non-commercial licenses (the notebook says so).
Each part is skipped when its folder is already there. No ffmpeg: soundfile
decodes flac and mp3, scipy resamples.

  cd ~/msw-wakeword && . .venv/bin/activate && python /mnt/c/MyProj/museStopWatch/tools/wakeword/data.py
"""
import os
import sys
import tarfile
import urllib.request
import zipfile
from math import gcd

import numpy as np
import scipy.io.wavfile
import scipy.signal
import soundfile as sf

RATE = 16000
HF = "https://huggingface.co/datasets"


def fetch(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return path
    print("downloading", url, flush=True)
    tmp = path + ".part"
    urllib.request.urlretrieve(url, tmp)
    os.replace(tmp, path)
    print("  ", os.path.getsize(path) // (1 << 20), "MB", flush=True)
    return path


def to_16k(data, rate):
    if data.ndim > 1:
        data = data.mean(axis=1)
    if rate != RATE:
        g = gcd(rate, RATE)
        data = scipy.signal.resample_poly(data, RATE // g, rate // g)
    return (np.clip(data, -1.0, 1.0) * 32767).astype(np.int16)


def convert_dir(src, dst, exts):
    os.makedirs(dst, exist_ok=True)
    n = bad = 0
    for root, _, files in os.walk(src):
        for f in files:
            if not f.lower().endswith(exts):
                continue
            out = os.path.join(dst, os.path.splitext(f)[0] + ".wav")
            if os.path.exists(out):
                continue
            try:
                data, rate = sf.read(os.path.join(root, f), dtype="float32")
                scipy.io.wavfile.write(out, RATE, to_16k(data, rate))
                n += 1
            except Exception as e:  # a broken file: skip it
                bad += 1
                if bad < 5:
                    print("  skip", f, e)
    print(f"  {dst}: {n} files ({bad} skipped)", flush=True)


def rirs():
    if os.path.isdir("mit_rirs"):
        return
    # The repository's files, not datasets' decoding (that wants torchcodec now).
    from huggingface_hub import snapshot_download

    raw = snapshot_download("davidscripka/MIT_environmental_impulse_responses", repo_type="dataset",
                            local_dir="mit_rirs_raw")
    convert_dir(raw, "mit_rirs.part", (".wav", ".flac"))
    os.rename("mit_rirs.part", "mit_rirs")
    print("  mit_rirs:", len(os.listdir("mit_rirs")), flush=True)


def audioset():
    if os.path.isdir("audioset_16k"):
        return
    # The notebook took bal_train09.tar; the repository now holds parquet
    # files (each row's audio as encoded bytes). Two of the 38 balanced ones.
    import io

    import pyarrow.parquet as pq
    from huggingface_hub import hf_hub_download

    os.makedirs("audioset_16k.part", exist_ok=True)
    n = bad = 0
    for part in ("09", "19"):
        path = hf_hub_download("agkphysics/AudioSet", f"data/bal_train/{part}.parquet", repo_type="dataset",
                               local_dir="audioset")
        table = pq.read_table(path, columns=["audio"])
        for i, a in enumerate(table.column("audio").to_pylist()):
            try:
                data, rate = sf.read(io.BytesIO(a["bytes"]), dtype="float32")
                scipy.io.wavfile.write(os.path.join("audioset_16k.part", f"{part}_{i}.wav"), RATE, to_16k(data, rate))
                n += 1
            except Exception:
                bad += 1
    print(f"  audioset_16k: {n} files ({bad} skipped)", flush=True)
    os.rename("audioset_16k.part", "audioset_16k")


def fma():
    if os.path.isdir("fma_16k"):
        return
    z = fetch(f"{HF}/mchl914/fma_xsmall/resolve/main/fma_xs.zip", "fma_xs.zip")
    with zipfile.ZipFile(z) as f:
        f.extractall("fma")
    convert_dir("fma", "fma_16k.part", (".mp3", ".wav"))
    os.rename("fma_16k.part", "fma_16k")


def negatives():
    os.makedirs("negative_datasets", exist_ok=True)
    for name in ("dinner_party", "dinner_party_eval", "no_speech", "speech"):
        if os.path.isdir(os.path.join("negative_datasets", name)):
            continue
        z = fetch(f"{HF}/kahrendt/microwakeword/resolve/main/{name}.zip", f"{name}.zip")
        with zipfile.ZipFile(z) as f:
            f.extractall("negative_datasets")
        os.remove(z)
        print("  negative_datasets/" + name, flush=True)


if __name__ == "__main__":
    parts = sys.argv[1:] or ["rirs", "audioset", "fma", "negatives"]
    for p in parts:
        print("==", p, flush=True)
        globals()[p]()
    print("data done")
