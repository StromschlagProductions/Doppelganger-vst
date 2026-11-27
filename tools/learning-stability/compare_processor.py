"""Compare diagnostic auto-stop modelling against the actual JUCE processor."""
from pathlib import Path
import argparse,json
import numpy as np
parser=argparse.ArgumentParser();parser.add_argument('work_dir',type=Path);args=parser.parse_args();root=args.work_dir
core={r['id']:r for r in map(json.loads,(root/'results.jsonl').read_text().splitlines())}
actual=[json.loads(s) for s in (root/'processor.jsonl').read_text().splitlines()];summary=[]
for p in actual:
 c=core[p['id']]
 row={'id':p['id'],'stop_difference_seconds':abs(c['used_seconds']-p['used_seconds']),
      'profile_max_difference_db':float(np.max(np.abs(np.array(p['profile'])-c['profile']))),
      'correction_max_difference_db':float(np.max(np.abs(np.array(p['correction'])-c['designs'][c['register']]['correction']))),
      'pitch_difference_hz':abs(p['pitch']-c['pitch']),'register_equal':p['register']==c['register']}
 summary.append(row)
 # EQ values pass through JUCE normalised parameter ranges in the real processor. Preserve/report the
 # observed micro-dB differences; they are not evidence of different sampling/convergence decisions.
 assert row['profile_max_difference_db']==0 and row['stop_difference_seconds']==0
 assert row['pitch_difference_hz']==0 and row['register_equal']
 assert row['correction_max_difference_db']<=1e-5
(root/'processor-comparison.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Processor comparisons passed:',len(summary))
