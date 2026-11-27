from pathlib import Path
import json,io,hashlib
import numpy as np
import pyarrow.parquet as pq
from scipy.io import wavfile
from scipy.signal import resample_poly
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('work_dir',type=Path)
args=parser.parse_args()
root=args.work_dir.resolve();root.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[2]
# Optional public-data download, pinned to the exact audited revision and verified against recorded hashes.
import urllib.request,hashlib
origin=json.loads((repo/'docs/learning-stability/source.json').read_text())
expected=json.loads((repo/'docs/learning-stability/provenance.json').read_text())
url='https://huggingface.co/api/datasets/'+origin['dataset']+'/revision/'+origin['revision']
meta=json.load(urllib.request.urlopen(url,timeout=30))
for speaker in ['bdl','slt']:
 remote=next(f['rfilename'] for f in meta['siblings'] if f['rfilename'].startswith('data/'+speaker+'-'))
 local=root/Path(remote).name
 if not local.exists():
  urllib.request.urlretrieve('https://huggingface.co/datasets/'+origin['dataset']+'/resolve/'+origin['revision']+'/'+remote+'?download=true',local)
 if hashlib.sha256(local.read_bytes()).hexdigest()!=expected['speakers'][speaker]['parquet_sha256']:
  raise ValueError('Corpus hash mismatch: '+speaker)
(root/'corpus-metadata.json').write_text(json.dumps(meta,indent=2))
corpora={}
for speaker in ['bdl','slt']:
 p=next(root.glob(speaker+'-*.parquet'))
 rows=pq.read_table(p).to_pylist()
 d={r['file'].split('-',1)[1]:r for r in rows}
 corpora[speaker]=d
ids=sorted(set(corpora['bdl'])&set(corpora['slt']))
rng=np.random.default_rng(20261127);rng.shuffle(ids)
groups=[ids[i*20:(i+1)*20] for i in range(12)]
manifest=[];provenance={'dataset':'MikhailT/cmu-arctic','revision':meta['sha'],'seed':20261127,'groups':groups,'speakers':{}}
for speaker,d in corpora.items():
 clips={}
 native_rates=set()
 for k in sum(groups,[]):
  sr,x=wavfile.read(io.BytesIO(d[k]['audio']['bytes']));native_rates.add(sr)
  assert x.ndim==1
  if np.issubdtype(x.dtype,np.integer):x=x.astype(np.float64)/(np.iinfo(x.dtype).max+1)
  clips[k]=x
 assert len(native_rates)==1
 sr=native_rates.pop();print(speaker,'native rate',sr,flush=True)
 provenance['speakers'][speaker]={'native_rate':sr,'utterances':len(d),'parquet_sha256':hashlib.sha256(next(root.glob(speaker+'-*.parquet')).read_bytes()).hexdigest()}
 for fs in [44100,48000]:
  path=root/f'{speaker}_{fs}.f32'; bounds=[];total=0
  with path.open('wb') as f:
   for i,group in enumerate(groups):
    samples=np.concatenate([resample_poly(clips[k],fs//np.gcd(fs,sr),sr//np.gcd(fs,sr)) for k in group]).astype('<f4')
    samples.tofile(f);bounds.append((total/fs,len(samples)/fs));total+=len(samples)
  provenance['speakers'][speaker][str(fs)]={'samples':total,'groups':bounds,'pcm_sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
  for i,(start,duration) in enumerate(bounds):
   manifest.append(f'{speaker}_{fs}_auto_{i:02d}\t{fs}\t{json.dumps(str(path))}\t{start:.12f}\t{duration:.12f}\tauto\t1\t512')
   if fs==48000:
    manifest.append(f'{speaker}_{fs}_short_{i:02d}\t{fs}\t{json.dumps(str(path))}\t{start:.12f}\t8\tfixed\t1\t512')
    manifest.append(f'{speaker}_{fs}_long_{i:02d}\t{fs}\t{json.dumps(str(path))}\t{start:.12f}\t{duration:.12f}\tfixed\t1\t512')
  if fs==48000:
   start,duration=bounds[0]
   for name,mode,gain,chunk in [('repeat','auto',1,512),('quiet','auto',10**(-24/20),512),('chunk137','fixed',1,137),('chunk512','fixed',1,512)]:
    manifest.append(f'{speaker}_control_{name}\t{fs}\t{json.dumps(str(path))}\t{start:.12f}\t{duration:.12f}\t{mode}\t{gain:.12f}\t{chunk}')
(root/'jobs.tsv').write_text('\n'.join(manifest)+'\n')
(root/'provenance.json').write_text(json.dumps(provenance,indent=2))
print('Jobs:',len(manifest),flush=True)
