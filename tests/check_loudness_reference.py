"""Independent optional acceptance: requires FFmpeg with its ebur128 filter."""
import json
import pathlib
import re
import subprocess
import sys

folder = pathlib.Path(sys.argv[1])
rows = []
for line in (folder / "measurements.tsv").read_text().splitlines():
    filename, channels, measured = line.split("\t")
    run = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", "-f", "f32le", "-ar", "48000",
         "-ac", channels, "-i", filename, "-af", "ebur128=peak=true", "-f", "null", "-"],
        capture_output=True, text=True, check=True)
    reference = float(re.findall(r"I:\s*(-?[\d.]+) LUFS", run.stderr)[-1])
    row = dict(case=pathlib.Path(filename).stem, channels=int(channels),
               plugin_lufs=float(measured), ffmpeg_lufs=reference,
               error_lu=round(float(measured) - reference, 6))
    if abs(row["error_lu"]) >= .12:
        raise RuntimeError(f"Meter mismatch: {row}")
    if "-normalised" in filename and abs(reference + 23) >= .2:
        raise RuntimeError(f"Target missed: {row}")
    rows.append(row)
if len(rows) != 14:
    raise RuntimeError(f"Expected 14 cases, got {len(rows)}")
(folder / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
print(json.dumps(rows, indent=2))
