# museStopWatch: source this before using the training environment (.venv):
#   . /mnt/c/MyProj/museStopWatch/tools/wakeword/env.sh
# TensorFlow 2.21 finds most of its pip CUDA libraries itself, but not
# libcusolver.so.11; every nvidia/*/lib goes on the library path.
W="${MSW_WW:-$HOME/msw-wakeword}"
cd "$W"
. .venv/bin/activate
LD_LIBRARY_PATH="$(printf '%s:' "$W"/.venv/lib/python3.10/site-packages/nvidia/*/lib)${LD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH
export TF_CPP_MIN_LOG_LEVEL=1
