from pathlib import Path
import json,itertools
import numpy as np
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('work_dir',type=Path)
args=parser.parse_args()
root=args.work_dir.resolve();root.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[2]
rows=[json.loads(s) for s in (root/'results.jsonl').read_text().splitlines()];by={r['id']:r for r in rows}
mic=[json.loads(s) for s in (root/'microphone.jsonl').read_text().splitlines()]
f=json.loads((root/'microphone-filter.json').read_text());grid=np.array(f['grid']);mask=(grid>=100)&(grid<=7000);known=np.array(f['response_db'])
def normalized(x):return x[mask]-np.mean(x[mask])
def resp(r,reg=None):return np.array(r['designs'][r['register'] if reg is None else reg]['response'])
def rms(x):return float(np.sqrt(np.mean(x*x)))
base_rms=rms(normalized(known));out=[]
for r in mic:
 speaker,_,i=r['id'].split('_');clean=by[f'{speaker}_48000_auto_{i}'];delta=resp(r)-resp(clean)
 out.append({'id':r['id'],'reg_clean':clean['register'],'reg_mic':r['register'],'before_rms_db':base_rms,'after_rms_db':rms(normalized(known+delta)),'amount_clean':clean['designs'][clean['register']]['amount'],'amount_mic':r['designs'][r['register']]['amount']})
rate=[]
for speaker in ['bdl','slt']:
 for i in range(12):
  a=by[f'{speaker}_44100_auto_{i:02d}'];b=by[f'{speaker}_48000_auto_{i:02d}'];d=np.abs(resp(a)-resp(b))[mask]
  rate.append({'id':speaker+f'_{i:02d}','mean_difference_db':float(np.mean(d)),'max_difference_db':float(max(d)),'stop_difference_seconds':abs(a['used_seconds']-b['used_seconds'])})
synthetics=[json.loads(s) for s in (root/'controls.jsonl').read_text().splitlines()];sb={r['id']:r for r in synthetics}
jumps=[]
for f0 in [157,222]:
 a=sb[f'synth_f{f0}_s5'];reg1=0 if f0==157 else 1;reg2=reg1+1
 d=np.abs(resp(a,reg1)-resp(a,reg2))[mask]
 jumps.append({'profile':a['id'],'registers':[reg1,reg2],'same_profile_different_reference_mean_db':float(np.mean(d)),'max_db':float(max(d))})
result={'microphone':out,'rate_consistency':rate,'isolated_reference_effect':jumps}
(root/'extra-summary.json').write_text(json.dumps(result,indent=2))
for speaker in ['bdl','slt']:
 rs=[r for r in out if r['id'].startswith(speaker)]
 print('MIC',speaker,'before',base_rms,'median after',np.median([r['after_rms_db'] for r in rs]),'range',min(r['after_rms_db'] for r in rs),max(r['after_rms_db'] for r in rs),'improved',sum(r['after_rms_db']<base_rms for r in rs),'of',len(rs),'register switches',sum(r['reg_clean']!=r['reg_mic'] for r in rs))
 print('RATES',speaker,'median',np.median([r['mean_difference_db'] for r in rate if r['id'].startswith(speaker)]),'maxmean',max(r['mean_difference_db'] for r in rate if r['id'].startswith(speaker)))
print('REFERENCE EFFECT',jumps)
