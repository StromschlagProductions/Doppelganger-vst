"""Plot the fitted EQs; the range is descriptive, not a confidence/quality score."""
from pathlib import Path
import argparse,json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=argparse.ArgumentParser();p.add_argument('work_dir',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root=a.work_dir;rows=[json.loads(x) for x in (root/'results.jsonl').read_text().splitlines()]
grid=np.array(json.loads((root/'microphone-filter.json').read_text())['grid']);mask=(grid>=100)&(grid<=7000)
fig,axs=plt.subplots(1,2,figsize=(12,4.5),sharey=True)
for ax,speaker,title in zip(axs,['bdl','slt'],['BDL · männliche Stimme','SLT · weibliche Stimme']):
 rs=[r for r in rows if r['id'].startswith(speaker+'_48000_auto_')]
 y=np.array([r['designs'][r['register']]['response'] for r in rs])[:,mask]
 ax.fill_between(grid[mask],y.min(axis=0),y.max(axis=0),color='#c7d4df',alpha=.65,label='Spannweite der 12 Kurven')
 for line in y:ax.plot(grid[mask],line,color='#71899b',lw=.8,alpha=.65)
 ax.plot(grid[mask],np.median(y,axis=0),color='#c7661b',lw=2.1,label='Median')
 ax.axhline(0,color='#3e4950',lw=.8);ax.set_xscale('log');ax.set_xlim(100,7000);ax.set_ylim(-12,12)
 ax.set_xticks([100,200,500,1000,2000,5000]);ax.set_xticklabels(['100','200','500','1k','2k','5k'])
 ax.set_title(title);ax.set_xlabel('Frequenz (Hz)');ax.grid(alpha=.2);ax.spines[['top','right']].set_visible(False)
axs[0].set_ylabel('Match-EQ (dB)');axs[1].legend(loc='lower left',fontsize=8,frameon=False)
fig.suptitle('Bestehender Autopilot: unterschiedliche Texte bei gleicher Aufnahmequelle',fontsize=12)
fig.text(.5,.015,'CMU ARCTIC · ursprüngl. 16 kHz, auf 48 kHz resampelt · ausgewertet: 100–7000 Hz · keine Hör- oder Klangbewertung',ha='center',fontsize=8,color='#444444')
fig.tight_layout(rect=[0,.04,1,.94]);a.output.parent.mkdir(parents=True,exist_ok=True);fig.savefig(a.output,dpi=170);plt.close(fig)
