"""Offline analysis of `frostbite_render --audit` output; numpy/scipy/matplotlib.

Levels in the renderer are peak dBFS. RMS and spectral measurements are not LUFS
or listening judgements. IR T20 fits are extrapolations, not direct 60 dB times.
"""
from pathlib import Path
import argparse
import csv
import json
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt, welch
from scipy.ndimage import median_filter
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def load(path):
    sr, data = wavfile.read(path)
    if np.issubdtype(data.dtype, np.integer):
        data = data.astype(np.float64) / (np.iinfo(data.dtype).max + 1.0)
    return sr, data


def db(x):
    return 20 * np.log10(np.maximum(np.abs(x), 1e-15))


def powerdb(x):
    return 10 * np.log10(np.maximum(x, 1e-30))


def write_csv(path, rows):
    with path.open('w', newline='', encoding='utf-8') as f:
        if rows:
            w = csv.DictWriter(f, fieldnames=rows[0].keys())
            w.writeheader()
            w.writerows(rows)


def decay(data, sr):
    e = np.sum(data * data, axis=1)
    edc = np.cumsum(e[::-1])[::-1]
    if not edc[0]:
        return np.full(len(e), -300.0), None, None
    edc_db = powerdb(edc / edc[0])
    # Sample sparsely for a stable fit without storing multi-million-row matrices.
    t = np.arange(len(e))[::128] / sr
    y = edc_db[::128]
    mask = (y >= -25) & (y <= -5)
    if mask.sum() < 20:
        return edc_db, None, None
    fit = np.polyfit(t[mask], y[mask], 1)
    prediction = np.polyval(fit, t[mask])
    r2 = 1 - np.sum((y[mask] - prediction) ** 2) / np.sum((y[mask] - y[mask].mean()) ** 2)
    return edc_db, -60 / fit[0] if fit[0] < 0 else None, r2


def analyse(folder):
    folder = Path(folder)
    harmonics = []
    for path in sorted(folder.glob('damage_*_sine*_*.wav')):
        if 'rms_minus' in path.stem:
            continue
        _, amount, signal, level, _ = path.stem.split('_')
        sr, data = load(path)
        x = data[-sr:, 0]  # exactly one second, coherent integer-frequency test
        frequency = int(signal.replace('sine', ''))
        amplitude = 2 * np.abs(np.fft.rfft(x)) / len(x)
        fundamental = amplitude[frequency]
        significant = np.zeros_like(amplitude, dtype=bool)
        significant[0] = True
        powers = []
        row = dict(file=path.name, damage=int(amount), input_peak_dbfs=int(level), sample_rate=sr,
                   frequency=frequency, rms_dbfs=float(db(np.sqrt(np.mean(x*x)))), fundamental_dbfs=float(db(fundamental)))
        for h in range(1, 13):
            hz = h * frequency
            value = amplitude[hz] if hz < len(amplitude) else None
            row[f'h{h}_dbc'] = float(db(value / fundamental)) if value is not None and fundamental > 0 else ''
        for hz in range(frequency, len(amplitude), frequency):
            significant[max(0, hz-1):hz+2] = True
            if hz > frequency:
                powers.append(amplitude[hz] ** 2)
        row['thd_inband_db'] = float(db(np.sqrt(sum(powers)) / fundamental))
        # At 7301 Hz these non-harmonic bins reveal folded products, also including
        # finite-precision/24-bit noise. This is a residual metric, not pure alias power.
        row['nonharmonic_residual_dbc'] = float(db(np.sqrt(np.sum(amplitude[~significant] ** 2)) / fundamental))
        harmonics.append(row)
    write_csv(folder / 'harmonics.csv', harmonics)
    ir_rows = []
    fig, ax = plt.subplots(1, 2, figsize=(11, 4))
    for path in sorted(folder.glob('reverb_*_*.wav'), key=lambda p: (int(p.stem.split('_')[2]), int(p.stem.split('_')[1]))):
        _, amount, _ = path.stem.split('_')
        sr, data = load(path)
        e = np.sum(data*data, axis=1)
        edc, t20, r2 = decay(data, sr)
        onset = np.flatnonzero(np.max(np.abs(data), axis=1) > 1e-6)
        early = e[:int(sr*.08)].sum()
        late = e[int(sr*.08):].sum()
        row = dict(file=path.name, abyss=int(amount), sample_rate=sr,
                   onset_ms=float(onset[0] / sr * 1000) if len(onset) else '',
                   energy_relative_input_db=float(powerdb(e.sum()/(2*10**(-6/10)))),
                   early80_late_db=float(powerdb(early/max(late, 1e-30))),
                   t20_extrapolated_rt60_s=t20, fit_r2=r2,
                   stereo_correlation=float(np.corrcoef(data.T)[0, 1]),
                   remaining_after_10s_db=float(powerdb(e[int(sr*10):].sum()/max(e.sum(), 1e-30))))
        for name, kind, cutoff in [('low', 'lowpass', 250), ('mid', 'bandpass', [250, 2000]), ('high', 'highpass', 2000)]:
            # Avoid pathological denormal arithmetic in long quantised silent tails.
            # Alternating -480 dBFS analysis-only dither is far below the 24-bit file floor.
            conditioned = data + (1e-24 * (1 - 2*(np.arange(len(data)) % 2)))[:, None]
            filtered = sosfilt(butter(4, cutoff, btype=kind, fs=sr, output='sos'), conditioned, axis=0)
            _, bt, br = decay(filtered, sr)
            row[f'{name}_energy_fraction'] = float(np.sum(filtered*filtered)/max(e.sum(), 1e-30))
            row[f'{name}_t20_s'] = bt
        # Density proxies state thresholds explicitly; neither is psychoacoustic echo density.
        for label, start, end in [('early', .03, .08), ('late', .2, .3)]:
            a = data[int(sr*start):int(sr*end)]
            rms = np.sqrt(np.mean(a*a))
            row[f'{label}_occupancy_above_local_rms_percent'] = float(100*np.mean(np.abs(a)>rms))
        freq, psd = welch(data[int(sr*.2):int(sr*2)], sr, nperseg=16384, axis=0)
        psd = psd.mean(axis=1)
        local = median_filter(psd, size=max(3, int(100/(sr/16384))|1))
        valid = (freq>=80)&(freq<=8000)&(psd>psd.max()*1e-8)
        prominence = powerdb(psd/np.maximum(local,1e-30))
        index = np.flatnonzero(valid)[np.argmax(prominence[valid])]
        row['late_psd_max_local_prominence_db'] = float(prominence[index])
        row['late_psd_prominent_frequency_hz'] = float(freq[index])
        ir_rows.append(row)
        if sr == 48000:
            stride = 128
            ax[0].plot(np.arange(len(edc))[::stride]/sr, edc[::stride], label=f'Abyss {amount}%')
            a = data[:sr*2, 0]
            freqs = np.fft.rfftfreq(len(a), 1/sr)
            power = np.abs(np.fft.rfft(a))**2
            # Log-band energy response (first two seconds); avoids suggesting a stationary transfer function.
            edges = np.geomspace(40, 20000, 70)
            centres = np.sqrt(edges[:-1]*edges[1:])
            bands = [np.mean(power[(freqs>=lo)&(freqs<hi)]) for lo, hi in zip(edges[:-1], edges[1:])]
            ax[1].semilogx(centres, powerdb(np.array(bands)), label=f'{amount}%')
    write_csv(folder / 'reverb.csv', ir_rows)
    ax[0].set(xlabel='Seconds', ylabel='Schroeder energy (dB)', ylim=(-65, 0), xlim=(0, 15), title='Isolated reverb, 48 kHz')
    ax[0].legend(fontsize=8)
    ax[1].set(xlabel='Hz', ylabel='IR band mean power (dB)', title='First 2 seconds, fixed input impulse')
    for a in ax: a.grid(alpha=.2)
    fig.tight_layout(); fig.savefig(folder/'reverb_analysis.png', dpi=160); plt.close(fig)
    fig, ax = plt.subplots(figsize=(8, 4))
    for row in harmonics:
        if row['sample_rate']==48000 and row['frequency']==1000 and row['input_peak_dbfs']==-18:
            ax.plot(range(1, 13), [row[f'h{h}_dbc'] for h in range(1, 13)], marker='.', label=f"Damage {row['damage']}%")
    ax.set(xlabel='Harmonic number (1 kHz input)', ylabel='dBc', ylim=(-100, 2), title='Isolated Damage: harmonics relative to fundamental')
    ax.grid(alpha=.2);ax.legend(fontsize=8);fig.tight_layout();fig.savefig(folder/'damage_harmonics.png',dpi=160);plt.close(fig)

    matrix = list(csv.DictReader((folder/'matrix.csv').open()))
    for row in matrix:
        for k in list(row)[2:]:row[k]=float(row[k])
    # Rank differences of same-source preset output. An affine level match removes
    # loudness as a trivial source of distance; zero means identical waveforms.
    similarities=[]
    preset_names=[r['name'].replace(' ', '_') for r in csv.DictReader((folder/'presets.csv').open())]
    for signal in ['piano', 'cello']:
        audio={name:load(folder/f'{name}_{signal}.wav')[1] for name in preset_names}
        for i, a in enumerate(preset_names):
            for b in preset_names[i+1:]:
                x,y=audio[a][:48000*4],audio[b][:48000*4]
                cosine=float(np.sum(x*y)/np.sqrt(np.sum(x*x)*np.sum(y*y)))
                similarities.append(dict(signal=signal,preset_a=a,preset_b=b,waveform_cosine=cosine,
                                         rms_matched_difference_db=float(db(np.sqrt(max(0,2-2*cosine))))))
    write_csv(folder/'preset_similarity.csv',similarities)
    chain_rows=[]
    for path in sorted(folder.glob('chain_ir_*.wav')):
        sr,data=load(path);energy=np.sum(data*data,axis=1);early=energy[:int(sr*.08)].sum();late=energy[int(sr*.08):].sum()
        _,tail_t20,r2=decay(data[int(sr*.08):],sr)
        chain_rows.append(dict(case=path.stem.replace('chain_ir_',''),
                               early80_late_db=float(powerdb(early/max(late,1e-30))),
                               late_energy_relative_input_db=float(powerdb(late/(2*10**(-6/10)))),
                               tail_after80ms_t20_s=tail_t20,tail_fit_r2=r2))
    write_csv(folder/'chain_ir.csv',chain_rows)
    summary=dict(matrix_rows=len(matrix), invalid_stage_samples=sum(float(r['nonfinite']) for r in csv.DictReader((folder/'stages.csv').open())),
                 peak_output=max(r['peak'] for r in matrix),
                 matched_target_rms_dbfs=-24, input_levels_are='sample peak dBFS',
                 warnings=['RMS is not perceived loudness/LUFS; no listening assessment.',
                           'Reverb T20 is -5 to -25 dB fit extrapolated to 60 dB, with finite 40s/24bit IR.',
                           'Nonharmonic residual includes aliasing, numerical error and 24bit quantisation.',
                           'Preset cosine is waveform similarity, not a perceptual identity score.'])
    (folder/'analysis_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    return summary


def compare(before, after):
    rows=[]
    for path in sorted(Path(after).glob('*.wav')):
        baseline=Path(before)/path.name
        if not baseline.exists():continue
        sr,x=load(baseline);sr2,y=load(path)
        if sr!=sr2 or x.shape!=y.shape:raise ValueError(f'Mismatched pair: {path.name}')
        diff=y-x
        rows.append(dict(file=path.name,max_absolute_difference=float(np.max(np.abs(diff))),
                         difference_rms_dbfs=float(db(np.sqrt(np.mean(diff*diff))))))
    write_csv(Path(after)/'before_after.csv',rows)
    return [r for r in rows if r['max_absolute_difference']>0]


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('folder');ap.add_argument('--before');args=ap.parse_args()
    print(json.dumps(analyse(args.folder),indent=2))
    if args.before:print(json.dumps({'changed_audio':compare(args.before,args.folder)},indent=2))
