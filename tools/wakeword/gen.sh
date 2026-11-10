#!/usr/bin/env bash
# museStopWatch: the wake word spoken by many synthetic English voices
# (piper-sample-generator, the LibriTTS-R generator: about 900 speakers, blended).
#
#   bash /mnt/c/MyProj/museStopWatch/tools/wakeword/gen.sh [samples per spelling]
# Output: ~/msw-wakeword/generated_samples/<n>.wav (16 kHz)
set -euo pipefail
W="${MSW_WW:-$HOME/msw-wakeword}"
N="${1:-2500}"
cd "$W"
export PATH="$HOME/.local/bin:$PATH"
# webrtcvad (a dependency) has no wheel and there is no compiler here: use
# its prebuilt fork, then the generator without dependencies.
if ! .venv-piper/bin/python -c "import piper_sample_generator" 2>/dev/null; then
    VIRTUAL_ENV="$W/.venv-piper" uv pip install webrtcvad-wheels audiomentations==0.33.0 piper-tts==1.3.0 'numpy>=2,<3'
    VIRTUAL_ENV="$W/.venv-piper" uv pip install --no-deps -e ./piper-sample-generator
fi
. .venv-piper/bin/activate
export PYTHONPATH="$W/piper-sample-generator"   # its piper_train package is not installed
rm -rf generated_samples generated_tmp
mkdir -p generated_samples
# Two spellings: the comma gives some voices a pause between the words.
i=0
for text in "hey muse" "hey, muse"; do
    python -m piper_sample_generator "$text" \
        --model piper-sample-generator/models/en_US-libritts_r-medium.pt \
        --max-samples "$N" --batch-size 100 \
        --slerp-weights 0.2 0.35 0.5 0.65 0.8 \
        --output-dir generated_tmp/$i
    i=$((i + 1))
done
# One folder, numbered.
n=0
for f in generated_tmp/*/*.wav; do
    mv "$f" generated_samples/$n.wav
    n=$((n + 1))
done
rm -rf generated_tmp
echo "gen done: $n samples in $W/generated_samples"
