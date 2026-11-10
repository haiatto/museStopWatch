"""museStopWatch: the wake word's training features (microWakeWord's
basic_training_notebook.ipynb, cells 5 and 7). Each synthetic sample is mixed
with room echo (mit_rirs), music (fma_16k) and noise (audioset_16k), then
turned into the spectrograms the model sees.

  . /mnt/c/MyProj/museStopWatch/tools/wakeword/env.sh
  python /mnt/c/MyProj/museStopWatch/tools/wakeword/features.py
Output: ~/msw-wakeword/generated_augmented_features/{training,validation,testing}/wakeword_mmap
"""
import os
import shutil

from microwakeword.audio.augmentation import Augmentation
from microwakeword.audio.clips import Clips
from microwakeword.audio.spectrograms import SpectrogramGeneration
from mmap_ninja.ragged import RaggedMmap

OUT = "generated_augmented_features"

clips = Clips(
    input_directory="generated_samples",
    file_pattern="*.wav",
    max_clip_duration_s=None,
    remove_silence=False,
    random_split_seed=10,
    split_count=0.1,
)
augmenter = Augmentation(
    augmentation_duration_s=3.2,
    augmentation_probabilities={
        "SevenBandParametricEQ": 0.1,
        "TanhDistortion": 0.1,
        "PitchShift": 0.1,
        "BandStopFilter": 0.1,
        "AddColorNoise": 0.1,
        "AddBackgroundNoise": 0.75,
        "Gain": 1.0,
        "RIR": 0.5,
    },
    impulse_paths=["mit_rirs"],
    background_paths=["fma_16k", "audioset_16k"],
    background_min_snr_db=-5,
    background_max_snr_db=10,
    min_jitter_s=0.195,
    max_jitter_s=0.205,
)

shutil.rmtree(OUT, ignore_errors=True)
for split, name, repeat, slide in (("training", "train", 2, 10), ("validation", "validation", 1, 10),
                                   ("testing", "test", 1, 1)):
    out_dir = os.path.join(OUT, split)
    os.makedirs(out_dir)
    # slide_frames 10: the same spectrogram shifted one frame at a time, as
    # streaming sees it; the test set uses the streaming model itself.
    spectrograms = SpectrogramGeneration(clips=clips, augmenter=augmenter, slide_frames=slide, step_ms=10)
    print("==", split, flush=True)
    RaggedMmap.from_generator(
        out_dir=os.path.join(out_dir, "wakeword_mmap"),
        sample_generator=spectrograms.spectrogram_generator(split=name, repeat=repeat),
        batch_size=100,
        verbose=True,
    )
print("features done")
