# Learning stability audit tools

Standalone diagnostic tools for [the German audit report](../../docs/LEARNING_STABILITY_AUDIT_2.3.md). No production target, plugin parameter or version is changed. The recorded plugin implementation is commit `a130fd4e803fa097d7a72b34d4a25737f5e798ac`; later DSP changes can produce different results.

## Recompute summaries from recorded measurements

From the repository root, use Python 3 with NumPy (and Matplotlib for the plot). Choose a writable scratch directory. The commands below use `work/stability`.

```sh
mkdir -p work/stability
python3 - <<'PY'
from pathlib import Path
import gzip, shutil
src = Path('docs/learning-stability')
dst = Path('work/stability')
for name in ['results', 'controls', 'microphone', 'processor']:
    (dst / (name + '.jsonl')).write_bytes(gzip.decompress((src / (name + '.jsonl.gz')).read_bytes()))
shutil.copyfile(src / 'microphone-filter.json', dst / 'microphone-filter.json')
PY
python3 tools/learning-stability/summarize.py work/stability
python3 tools/learning-stability/extra_metrics.py work/stability
python3 tools/learning-stability/compare_processor.py work/stability
python3 tools/learning-stability/plot.py work/stability work/stability/curves.png
```

`median_pair_mean_db` is the median over all pairwise mean absolute frequency-response differences on the reference grid, restricted to 100–7000 Hz. `worst_pair_max_db` is the local maximum **within the pair having the largest mean**, not necessarily the global local maximum across every pair. The report explicitly gives the latter as well. These are EQ-shape differences, not LUFS or perceptual quality scores.

## Repeat the signal analysis

Requires a C++17 compiler and Python packages NumPy, SciPy and PyArrow. `prepare.py` downloads approximately 200 MB from the pinned public CMU ARCTIC mirror, verifies the recorded Parquet hashes, selects the fixed sentence groups and produces mono little-endian float PCM plus manifests. Allow additional scratch space for PCM (roughly a few GB including controls). It does not upload audio or transcripts. Observe the source dataset's terms when reusing recordings.

```sh
mkdir -p work/stability
g++ -std=c++17 -O2 -pthread tools/learning-stability/audit_learning.cpp -o work/stability/analyse
g++ -std=c++17 -O2 tools/learning-stability/controls.cpp -o work/stability/controls
python3 tools/learning-stability/prepare.py work/stability
work/stability/analyse work/stability/jobs.tsv > work/stability/results.jsonl
work/stability/controls work/stability
work/stability/analyse work/stability/controls.tsv > work/stability/controls.jsonl
python3 tools/learning-stability/microphone.py work/stability
work/stability/analyse work/stability/microphone.tsv > work/stability/microphone.jsonl
python3 tools/learning-stability/summarize.py work/stability
python3 tools/learning-stability/extra_metrics.py work/stability
```

Manifests contain whitespace-separated fields: ID, sample rate, quoted absolute PCM path, start seconds, duration seconds, `auto` or `fixed`, linear gain, chunk size. `fixed` consumes the supplied interval and deliberately bypasses automatic convergence stopping. That comparison is a diagnostic capability, not a new user-facing plugin feature. Generated manifests are trusted local inputs; do not run arbitrary unreviewed manifests.

Original audio is **16 kHz**. Resampling to 44.1/48 kHz only exercises those plugin paths; it cannot validate high-frequency source behaviour. Restrict these corpus comparisons to the documented 100–7000 Hz band. Different sentences from two speakers are not a representative test of all voices, languages or microphones.

## Actual JUCE processor cross-check

Use the normal project JUCE/dependency setup and a separate Release build. Add the optional audit target using the CMake project hook (CMake 3.19 or newer); the production CMakeLists need no edit:

```sh
cmake -S . -B work/stability-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PROJECT_Doppelganger_INCLUDE="$PWD/tools/learning-stability/register.cmake"
cmake --build work/stability-build --target stability_processor --config Release
python3 - <<'PY'
from pathlib import Path
root = Path('work/stability')
selected = [line for line in (root / 'jobs.tsv').read_text().splitlines()
            if '_auto_' in line.split()[0] and line.split()[0].endswith(('_00', '_04', '_09'))]
assert len(selected) == 12
(root / 'processor.tsv').write_text('\n'.join(selected) + '\n')
PY
```

Run the resulting `stability_processor` console executable with `work/stability/processor.tsv` as its sole argument and redirect stdout to `work/stability/processor.jsonl`. Its location varies by generator/platform (for example `work/stability-build/stability_processor_artefacts/Release/stability_processor`). Then run `compare_processor.py` as above.

The harness executes the actual mono `processBlock`, timer callback and autopilot EQ design. It polls the timer at approximately 33 ms of processed audio and stops before the subsequent output-loudness calibration. It does not test DAW scheduling, real-time CPU load, full-chain rendered audio quality or GUI behaviour. The comparator checks exact learning decisions and allows at most 0.00001 dB correction difference for JUCE's normalized float parameter path; observed differences are retained in the report, not rounded away.

## Recorded environment

Linux x86-64, GCC 14.2, Python 3.12.14. Corpus preparation: NumPy 2.5.5, SciPy 1.17.0, PyArrow 26.0.1. Summary/plot processing: NumPy 2.3.5, SciPy 1.17.0, Matplotlib 3.10.8. JUCE is the project dependency. Deterministic sentence selection, source hashes and float PCM hashes are recorded in `docs/learning-stability/provenance.json`. Cross-platform floating-point or dependency differences may prevent byte-identical output.

## Original reference recordings

The subsequently supplied reference WAVs are assessed in [a separate report](../../docs/REFERENCE_RECORDINGS_AUDIT_2.3.md). `check_references.py` checks locally supplied files against the embedded curves, inspects five-second windows and can compare four cases against the actual processor. It additionally requires SoundFile (recorded run: 0.14.0). See that report for the invocation. Source WAVs and PCM remain local; the repository contains only aggregate measurements and hashes.
