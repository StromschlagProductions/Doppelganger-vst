from pathlib import Path
import json,itertools
import numpy as np
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('work_dir',type=Path)
args=parser.parse_args()
root=args.work_dir.resolve();root.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[2]
rows=[json.loads(s) for s in (root/'results.jsonl').read_text().splitlines()]
controls=[json.loads(s) for s in (root/'controls.jsonl').read_text().splitlines()]
by={r['id']:r for r in rows}
# Exact grid from the checked-in reference, no rounded reconstruction.
import re
text=(repo/'Source/ReferenceCurves.h').read_text()
grid=np.array([float(s) for s in re.search(r'kRefGridHz\[kRefGridPoints\] = \{(.*?)\};',text,re.S).group(1).replace('f','').split(',') if s.strip()])
mask=(grid>=100)&(grid<=7000)
def response(row,reg=None):return np.array(row['designs'][row['register'] if reg is None else reg]['response'])
def distance(a,b):return float(np.mean(np.abs(np.array(a)[mask]-np.array(b)[mask])))
summary={'evaluation_hz':[100,7000],'cohorts':[],'controls':[]}
for speaker in ['bdl','slt']:
 for fs in [44100,48000]:
  for kind in ['auto','short','long']:
   rs=[r for r in rows if r['id'].startswith(f'{speaker}_{fs}_{kind}_')]
   if not rs:continue
   good=[r for r in rs if r['valid']]
   modal=int(np.argmax(np.bincount([r['register'] for r in good],minlength=3)))
   pairs=[(distance(response(a),response(b)),a['id'],b['id'],float(np.max(np.abs(response(a)[mask]-response(b)[mask])))) for a,b in itertools.combinations(good,2)]
   fixed=[distance(response(a,modal),response(b,modal)) for a,b in itertools.combinations(good,2)]
   worst=max(pairs)
   item={'speaker':speaker,'fs':fs,'kind':kind,'valid':len(good),'total':len(rs),'stopped':sum(r['stopped'] for r in good),'speech_range':[min(r['speech_seconds'] for r in good),max(r['speech_seconds'] for r in good)],'pitch_range':[min(r['pitch'] for r in good),max(r['pitch'] for r in good)],'register_counts':np.bincount([r['register'] for r in good],minlength=3).tolist(),'median_pair_mean_db':float(np.median([p[0] for p in pairs])),'worst_pair_mean_db':worst[0],'worst_pair_ids':list(worst[1:3]),'worst_pair_max_db':worst[3],'fixed_register':modal,'fixed_median_pair_mean_db':float(np.median(fixed)),'fixed_worst_pair_mean_db':max(fixed)}
   summary['cohorts'].append(item)
for speaker in ['bdl','slt']:
 a=by[f'{speaker}_48000_auto_00'];b=by[f'{speaker}_control_repeat'];q=by[f'{speaker}_control_quiet'];x=by[f'{speaker}_control_chunk137'];y=by[f'{speaker}_control_chunk512']
 summary['controls'].append({'speaker':speaker,'repeat_identical':a['profile']==b['profile'] and a['designs']==b['designs'],'chunk_identical':x['profile']==y['profile'] and x['designs']==y['designs'],'gain_24db_mean_difference_db':distance(response(a),response(q)),'gain_stopped_seconds_difference':a['used_seconds']-q['used_seconds']})
summary['synthetic']=[{'id':r['id'],'pitch':r['pitch'],'register':r['register'],'speech':r['speech_seconds'],'stopped':r['stopped'],'amount':r['designs'][r['register']]['amount']} for r in controls]
(root/'summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
