"""Copy the parts of sanoTTS-jp that the firmware builds into components/sanotts/upstream.

    git clone https://github.com/ayutaz/sanoTTS-jp .tools/sanotts-jp     (or git pull there)
    .venv\\Scripts\\python.exe tools\\sync_sanotts.py

The files are copied unchanged (upstream says not to edit them; our own code
lives next to them in components/sanotts). The commit is written to
upstream/SOURCE.txt. The model and dictionary blobs are not in the git repo;
they come from the official v1.2.0 release (see components/sanotts/model/README.md).
"""
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / ".tools" / "sanotts-jp"
DST = ROOT / "muse-gadget-sdk" / "esp32" / "components" / "sanotts" / "upstream"

CSRC = """
saanotts.c saanotts.h saanotts_int8.c saanotts_int8.h saanotts_stream.c saanotts_stream.h
saanotts_internal.h saan_prof.h fft.c fft.h g2p.c g2p.h g2p_table.h token_table.h erf_table.h
dan_table.h jdict.c jdict.h accent.c accent.h njd_rules.c njd_rules.h label_ids.c label_ids.h
oj_heap_psram.h
""".split()
ESP32 = {
    "esp32/components/saanotts_core/oj_heap_psram.c": "oj_heap_psram.c",
    "esp32/components/saanotts_core/saan_port_esp32.h": "saan_port_esp32.h",
    "esp32/main/saan_kanji.c": "saan_kanji.c",
    "esp32/main/saan_kanji.h": "saan_kanji.h",
    "esp32/main/saan_pcm.c": "saan_pcm.c",
    "esp32/main/saan_pcm.h": "saan_pcm.h",
}
TOP = ["LICENSE", "LICENSE-APACHE-2.0.txt", "LICENSE-MODEL.md", "MODEL_CARD.md", "NOTICE.md"]


def main():
    if not (SRC / "csrc" / "saanotts.c").exists():
        sys.exit(f"no sanoTTS-jp clone at {SRC}")
    if DST.exists():
        shutil.rmtree(DST)
    (DST / "openjtalk").mkdir(parents=True)
    for f in CSRC:
        shutil.copy2(SRC / "csrc" / f, DST / f)
    for f in (SRC / "csrc" / "openjtalk").iterdir():
        if f.is_file():
            shutil.copy2(f, DST / "openjtalk" / f.name)
    for a, b in ESP32.items():
        shutil.copy2(SRC / a, DST / b)
    for f in TOP:
        shutil.copy2(SRC / f, DST / f)
    rev = subprocess.run(["git", "-C", str(SRC), "log", "-1", "--format=%H %cs %s"],
                         capture_output=True, text=True).stdout.strip()
    (DST / "SOURCE.txt").write_text(
        "https://github.com/ayutaz/sanoTTS-jp\n" + rev + "\n"
        "Copied unchanged by tools/sync_sanotts.py. Do not edit; re-run the script.\n",
        encoding="utf-8")
    n = sum(1 for _ in DST.rglob("*") if _.is_file())
    print(f"copied {n} files from {rev}")


if __name__ == "__main__":
    main()
