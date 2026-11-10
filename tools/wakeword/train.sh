#!/usr/bin/env bash
# museStopWatch: trains the wake word model (microWakeWord's
# basic_training_notebook.ipynb, cells 9 and 10) and copies the result here.
#
#   bash /mnt/c/MyProj/museStopWatch/tools/wakeword/train.sh [steps]
# Output: ~/msw-wakeword/trained_models/hey_muse/, and
#   work/wakeword/hey_muse.tflite in the project (to try before it goes into the firmware)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
STEPS="${1:-10000}"
. "$HERE/env.sh"

python - "$STEPS" <<'EOF'
import sys, yaml
neg = lambda d, w, s="random": {"features_dir": "negative_datasets/" + d, "sampling_weight": w,
                                "penalty_weight": 1.0, "truth": False, "truncation_strategy": s, "type": "mmap"}
config = {
    "window_step_ms": 10,
    "train_dir": "trained_models/hey_muse",
    "features": [
        {"features_dir": "generated_augmented_features", "sampling_weight": 2.0, "penalty_weight": 1.0,
         "truth": True, "truncation_strategy": "truncate_start", "type": "mmap"},
        neg("speech", 10.0),
        neg("dinner_party", 10.0),
        neg("no_speech", 5.0),
        neg("dinner_party_eval", 0.0, "split"),   # validation and testing only
    ],
    "training_steps": [int(sys.argv[1])],
    "positive_class_weight": [1],
    "negative_class_weight": [20],
    "learning_rates": [0.001],
    "batch_size": 128,
    "time_mask_max_size": [0],
    "time_mask_count": [0],
    "freq_mask_max_size": [0],
    "freq_mask_count": [0],
    "eval_step_interval": 500,
    "clip_duration_ms": 1500,
    "target_minimization": 0.9,
    "minimization_metric": None,
    "maximization_metric": "average_viable_recall",
}
yaml.dump(config, open("training_parameters.yaml", "w"))
EOF

python -m microwakeword.model_train_eval \
    --training_config='training_parameters.yaml' \
    --train 1 \
    --restore_checkpoint 1 \
    --test_tf_nonstreaming 0 \
    --test_tflite_nonstreaming 0 \
    --test_tflite_nonstreaming_quantized 0 \
    --test_tflite_streaming 0 \
    --test_tflite_streaming_quantized 1 \
    --use_weights "best_weights" \
    mixednet \
    --pointwise_filters "64,64,64,64" \
    --repeat_in_block "1, 1, 1, 1" \
    --mixconv_kernel_sizes '[5], [7,11], [9,15], [23]' \
    --residual_connection "0,0,0,0" \
    --first_conv_filters 32 \
    --first_conv_kernel_size 5 \
    --stride 3

T=trained_models/hey_muse/tflite_stream_state_internal_quant/stream_state_internal_quant.tflite
mkdir -p "$HERE/../../work/wakeword"
cp "$T" "$HERE/../../work/wakeword/hey_muse.tflite"
ls -la "$HERE/../../work/wakeword/hey_muse.tflite"
echo "train done"
