"""Summarise V2 evidence; reuse established audit analysis and deterministic WAVs."""
import csv,json,argparse
from pathlib import Path
import numpy as np
from analyse_audit import load,write_csv,compare,db

def main(root):
    root=Path(root);out=root/'analysis';out.mkdir(exist_ok=True)
    golden=compare(root/'legacy',root/'legacy_check')
    all_pairs=list(csv.DictReader((root/'legacy_check'/'before_after.csv').open()))
    result=dict(legacy_pairs=len(all_pairs),legacy_changed=len(golden),legacy_largest_difference=max(float(r['max_absolute_difference']) for r in all_pairs))
    rows=[]
    for folder in ('legacy','after','audition'):
        for r in csv.DictReader((root/folder/'matrix.csv').open()):
            r=dict(r);r['collection']=folder;r['crest_db']=float(r['peak_dbfs'])-float(r['rms_dbfs']);rows.append(r)
    write_csv(out/'levels.csv',rows)
    write_csv(out/'frequency_response.csv',[r for r in rows if r['signal'] in ('sine100','sine1000','sine5000')])
    result['nonfinite_matrix_samples']=sum(int(r['nonfinite']) for r in rows)
    result['peak']=max(float(r['peak']) for r in rows)
    result['full_scale_samples']=sum(int(r['clips']) for r in rows)
    result['default_cello_gain']={f:float(next(r['gain_db'] for r in rows if r['collection']==f and r['case']=='init' and r['signal']=='cello' and float(r['input_peak_dbfs'])==-18)) for f in ('legacy','after')}
    imd=[]
    frequencies=(83,251,997,3001,7001)
    for path in sorted((root/'audition').glob('imd_*.wav')):
        sr,x=load(path);spectrum=2*np.abs(np.fft.rfft(x[-sr:,0]))/sr
        primary=sum(spectrum[f]**2 for f in frequencies);mask=np.ones(len(spectrum),dtype=bool);mask[0]=False
        for f in frequencies:
            for h in range(f,len(spectrum),f):mask[max(0,h-1):h+2]=False
        imd.append(dict(file=path.name,nonharmonic_product_dbc=float(db(np.sqrt(np.sum(spectrum[mask]**2)/max(primary,1e-30)))),rms_dbfs=float(db(np.sqrt(np.mean(x[-sr:,0]**2))))))
    write_csv(out/'intermodulation.csv',imd)
    stage_rows=list(csv.DictReader((root/'audition'/'stages.csv').open()))
    peak_summary=[]
    for r in stage_rows:
        if r['stage']=='mixed':
            protected=next(v for v in stage_rows if v['stage']=='protected' and all(v[k]==r[k] for k in ('case','signal','input_peak_dbfs')))
            peak_summary.append(dict(preset=r['case'],signal=r['signal'],input_peak_dbfs=r['input_peak_dbfs'],pre_safety_peak=float(r['peak']),protected_peak=float(protected['peak']),safety_rms_change_db=float(protected['rms_dbfs'])-float(r['rms_dbfs'])))
    write_csv(out/'preset_headroom.csv',peak_summary)
    result['max_routine_pre_safety_peak']=max(r['pre_safety_peak'] for r in peak_summary)
    result['max_routine_safety_rms_change_db']=min(r['safety_rms_change_db'] for r in peak_summary)
    result['notes']=['Synthetic deterministic sources; not recordings or subjective listening.', 'RMS matching is not LUFS or perceived loudness matching.', 'IMD residual excludes integer harmonics of every input tone; includes aliases/quantisation.', 'Golden comparisons use preserved post-fix Legacy executable.']
    (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
    if result['legacy_changed'] or result['nonfinite_matrix_samples']:raise SystemExit(1)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root');main(p.parse_args().root)
