from pathlib import Path
import numpy as np,json,re
from scipy.signal import lfilter,freqz
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('work_dir',type=Path)
args=parser.parse_args()
root=args.work_dir.resolve();root.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[2]
text=(repo/'Source/ReferenceCurves.h').read_text()
grid=np.array([float(s) for s in re.search(r'kRefGridHz\[kRefGridPoints\] = \{(.*?)\};',text,re.S).group(1).replace('f','').split(',') if s.strip()])
fs=48000;A=10**(-6/40);w=2*np.pi*3000/fs;c=np.cos(w);alpha=np.sin(w)/2*np.sqrt(2);sq=2*np.sqrt(A)*alpha
b=A*np.array([(A+1)+(A-1)*c+sq,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-sq]);a=np.array([(A+1)-(A-1)*c+sq,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-sq]);b/=a[0];a/=a[0]
A=10**(4/40);w=2*np.pi*350/fs;c=np.cos(w);alpha=np.sin(w)/(2*.7)
b2=np.array([1+alpha*A,-2*c,1-alpha*A]);a2=np.array([1+alpha/A,-2*c,1-alpha/A]);b2/=a2[0];a2/=a2[0]
known=20*np.log10(np.abs(freqz(b,a,worN=grid,fs=fs)[1]*freqz(b2,a2,worN=grid,fs=fs)[1]))
(root/'microphone-filter.json').write_text(json.dumps({'fs':fs,'grid':grid.tolist(),'response_db':known.tolist(),'description':'Artificial fixed microphone colour: +4 dB bell at 350 Hz Q=.7 and -6 dB high shelf at 3 kHz (RBJ)'},indent=2))
prov=json.loads((root/'provenance.json').read_text());jobs=[]
for speaker in ['bdl','slt']:
 x=np.fromfile(root/f'{speaker}_48000.f32',dtype='<f4');y=lfilter(b2,a2,lfilter(b,a,x)).astype('<f4')
 p=root/f'{speaker}_mic_48000.f32';y.tofile(p)
 print(speaker,'linear-float peak before/after',max(abs(x)),max(abs(y)))
 for i,(start,duration) in enumerate(prov['speakers'][speaker]['48000']['groups']):jobs.append(f'{speaker}_mic_{i:02d}\t48000\t{json.dumps(str(p))}\t{start:.12f}\t{duration:.12f}\tauto\t1\t512')
(root/'microphone.tsv').write_text('\n'.join(jobs)+'\n')
