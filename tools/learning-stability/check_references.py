"""Inspect three local reference WAVs without changing the embedded targets or uploading audio."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import subprocess

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

parser = argparse.ArgumentParser()
parser.add_argument('work_dir', type=Path)
parser.add_argument('--tief', type=Path, required=True)
parser.add_argument('--mittel', type=Path, required=True)
parser.add_argument('--hoch', type=Path, required=True)
parser.add_argument('--analyser', type=Path, required=True)
parser.add_argument('--processor', type=Path)
args = parser.parse_args()
root = args.work_dir.resolve()
root.mkdir(parents=True, exist_ok=True)
repo = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('reference_generator', repo / 'tools/make_reference_curves.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)
# Importing the generator does not execute main() or rewrite ReferenceCurves.h.
import re
header = (repo / 'Source/ReferenceCurves.h').read_text()
grid = np.array([float(x.strip().removesuffix('f')) for x in re.search(
    r'kRefGridHz\[kRefGridPoints\] = \{(.*?)\};', header, re.S).group(1).split(',') if x.strip()])
band = (grid >= 100) & (grid <= 8000)
metadata = []
jobs = []
starts_by_id = {}
for name in ['tief', 'mittel', 'hoch']:
    path = getattr(args, name).resolve()
    x, fs = sf.read(path, always_2d=True)
    info = sf.info(path)
    mono = gen.load_mono_48k(path)
    duration = len(mono) / gen.SR
    if duration < 5:
        raise ValueError('At least five seconds required: ' + name)
    curve, cutoff = gen.curve_on_grid(path)
    embedded = np.array([float(v.strip().removesuffix('f')) for v in re.search(
        'kRef' + name.title() + r'Db\[128\] = \{(.*?)\};', header, re.S).group(1).split(',') if v.strip()])
    peak = np.max(abs(x))
    rms = np.sqrt(np.mean(x * x))
    true_peak = np.max(abs(resample_poly(x, 4, 1, axis=0)))
    metadata.append({
        'reference': name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'fs': fs, 'channels': info.channels, 'subtype': info.subtype,
        'samples': len(x), 'duration_seconds': len(x) / fs,
        'peak_dbfs': float(20 * np.log10(peak)),
        'truepeak_4x_dbtp': float(20 * np.log10(true_peak)),
        'rms_dbfs': float(20 * np.log10(rms)),
        'crest_db': float(20 * np.log10(peak / rms)),
        'samples_abs_ge_0999': int(np.sum(abs(x) >= .999)),
        'generated_bandwidth_hz': cutoff,
        'embedded_max_rounding_difference_db': float(np.max(abs(curve - embedded))),
        'rounded_curve_matches_embedded': bool(np.all(np.round(curve, 3) == embedded)),
    })
    pcm = root / (name + '.f32')
    mono.astype('<f4').tofile(pcm)
    for mode in ['fixed', 'auto']:
        jobs.append(f'{name}_full_{mode}\t48000\t{json.dumps(str(pcm))}\t0\t{duration:.12f}\t{mode}\t1\t512')
    starts = list(np.arange(0, duration - 5 + 1e-8, 1))
    if abs(starts[-1] - (duration - 5)) > .01:
        starts.append(duration - 5)
    for i, start in enumerate(starts):
        case = f'{name}_5s_{i:02d}'
        starts_by_id[case] = float(start)
        jobs.append(f'{case}\t48000\t{json.dumps(str(pcm))}\t{start:.12f}\t5\tfixed\t1\t512')
manifest = root / 'references.tsv'
manifest.write_text('\n'.join(jobs) + '\n')
with (root / 'reference-core.jsonl').open('w') as out:
    subprocess.run([str(args.analyser.resolve()), str(manifest)], stdout=out, check=True)
rows = [json.loads(s) for s in (root / 'reference-core.jsonl').read_text().splitlines()]
by_id = {r['id']: r for r in rows}

def response(row, reg):
    return np.array(row['designs'][reg]['response'])

def distance(a, b):
    delta = abs(a - b)[band]
    return {'mean_abs_db': float(np.mean(delta)), 'max_abs_db': float(np.max(delta)),
            'max_hz': float(grid[band][np.argmax(delta)])}

result = {'evaluation_hz': [100, 8000], 'references': [], 'processor_checks': []}
for reg, meta in enumerate(metadata):
    name = meta['reference']
    full = by_id[name + '_full_fixed']
    meta.update({
        'full_pitch_hz': full['pitch'], 'full_register': full['register'],
        'full_speech_seconds': full['speech_seconds'],
        'full_learned_lufs_after_default_lowcut': full['lufs'],
        'auto_stopped_before_file_end': by_id[name + '_full_auto']['stopped'],
        'self_match_eq': distance(response(full, reg), np.zeros(len(grid))),
        'five_second_windows': [],
    })
    for row in rows:
        if not row['id'].startswith(name + '_5s_'):
            continue
        meta['five_second_windows'].append({
            'id': row['id'], 'start_seconds': starts_by_id[row['id']],
            'pitch_hz': row['pitch'], 'auto_register': row['register'],
            'speech_seconds': row['speech_seconds'],
            'amount_fixed_register_percent': row['designs'][reg]['amount'],
            'amount_auto_register_percent': row['designs'][row['register']]['amount'],
            'fixed_register_vs_full_eq': distance(response(row, reg), response(full, reg)),
            'auto_vs_fixed_register_eq': distance(response(row, row['register']), response(row, reg)),
        })
    result['references'].append(meta)
if args.processor:
    selected = [s for s in jobs if s.split()[0].endswith('_full_fixed')]
    selected.append(next(s for s in reversed(jobs) if s.split()[0].startswith('mittel_5s_')))
    processor_manifest = root / 'reference-processor.tsv'
    processor_manifest.write_text('\n'.join(selected) + '\n')
    with (root / 'reference-processor.jsonl').open('w') as out:
        subprocess.run([str(args.processor.resolve()), str(processor_manifest)], stdout=out, check=True)
    for line in (root / 'reference-processor.jsonl').read_text().splitlines():
        actual = json.loads(line)
        core = by_id[actual['id']]
        check = {
            'id': actual['id'],
            'profile_max_difference_db': float(np.max(abs(np.array(actual['profile']) - core['profile']))),
            'pitch_equal': actual['pitch'] == core['pitch'],
            'register_equal': actual['register'] == core['register'],
            'stop_equal': actual['used_seconds'] == core['used_seconds'],
            'correction_max_difference_db': float(np.max(abs(np.array(actual['correction']) - core['designs'][core['register']]['correction']))),
        }
        assert check['profile_max_difference_db'] == 0
        assert check['pitch_equal'] and check['register_equal'] and check['stop_equal']
        assert check['correction_max_difference_db'] <= 1e-5
        result['processor_checks'].append(check)
(root / 'reference-summary.json').write_text(json.dumps(result, indent=2) + '\n')
for meta in result['references']:
    print(meta['reference'], 'matches embedded:', meta['rounded_curve_matches_embedded'],
          'seconds:', round(meta['duration_seconds'], 3), 'pitch:', round(meta['full_pitch_hz'], 2),
          'window registers:', [r['auto_register'] for r in meta['five_second_windows']])
