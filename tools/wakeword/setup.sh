#!/usr/bin/env bash
# museStopWatch: the microWakeWord training environment, in WSL (Ubuntu).
# Follows microWakeWord's notebooks/basic_training_notebook.ipynb.
# Everything goes under $MSW_WW (default ~/msw-wakeword); no sudo, nothing global:
# uv goes to ~/.local/bin and fetches its own Python 3.10.
#
# Two environments, because torch's CUDA 13 libraries and TensorFlow's CUDA 12
# ones overwrite each other in one site-packages:
#   .venv-piper  torch + piper-sample-generator: the synthetic voices
#   .venv        TensorFlow (GPU) + microWakeWord: features and training
#
#   bash /mnt/c/MyProj/museStopWatch/tools/wakeword/setup.sh
#   MSW_WW_CLEAN=1 ... to build both environments again
set -euo pipefail
W="${MSW_WW:-$HOME/msw-wakeword}"
mkdir -p "$W"
cd "$W"

if ! command -v uv >/dev/null && [ ! -x "$HOME/.local/bin/uv" ]; then
    curl -LsSf https://astral.sh/uv/install.sh | env UV_NO_MODIFY_PATH=1 sh
fi
export PATH="$HOME/.local/bin:$PATH"

if [ "${MSW_WW_CLEAN:-0}" = 1 ]; then
    rm -rf .venv .venv-piper
fi

[ -d microWakeWord ] || git clone https://github.com/kahrendt/microWakeWord
[ -d piper-sample-generator ] || git clone https://github.com/rhasspy/piper-sample-generator

# Synthetic voices.
[ -d .venv-piper ] || uv venv --python 3.10 .venv-piper
VIRTUAL_ENV="$W/.venv-piper" uv pip install torch torchaudio piper-phonemize-cross==1.2.1 numpy scipy soundfile
mkdir -p piper-sample-generator/models
M=piper-sample-generator/models/en_US-libritts_r-medium.pt
[ -s "$M" ] || wget -q -O "$M" https://github.com/rhasspy/piper-sample-generator/releases/download/v2.0.0/en_US-libritts_r-medium.pt

# Features and training.
[ -d .venv ] || uv venv --python 3.10 .venv
export VIRTUAL_ENV="$W/.venv"
uv pip install 'git+https://github.com/whatsnowplaying/audio-metadata@d4ebb238e6a401bb1a5aaaac60c9e2b3cb30929f'
uv pip install -e ./microWakeWord
TFV=$(.venv/bin/python -c "import importlib.metadata as m; print(m.version('tensorflow'))")
# datasets 4 decodes audio with torchcodec (and FFmpeg); 3.x uses soundfile.
uv pip install "tensorflow[and-cuda]==$TFV" "datasets<4" librosa soundfile scipy pyarrow tensorboard

.venv-piper/bin/python -c "import torch; print('torch', torch.__version__, 'CUDA:', torch.cuda.is_available())"
(. "$(dirname "$0")/env.sh" && python -c "import tensorflow as tf; print('tensorflow', tf.__version__, 'GPU:', tf.config.list_physical_devices('GPU'))")
echo "setup done: $W"
