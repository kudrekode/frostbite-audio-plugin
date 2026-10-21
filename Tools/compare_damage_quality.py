"""Offline exact V2 Broken curve: ADAA vs point-shaper FIR oversampling.
Float64 model, Damage=100%, Extra Drive=0. Residual is not pure alias power.
No perceived-quality claim and no C++ oversampling CPU/latency measurement.
"""
import json
import numpy as np
import scipy
from scipy.signal import firwin,lfilter,resample_poly
PI=np.pi
FREQUENCY=7301
DRIVE=10**(32/20)
def shape(x):
    return np.clip(x+.18,-.72,.92)-.18+np.where(np.abs(x)<1.2,.12*np.sin(PI*x/1.2),0.)
def primitive(x):
    t=x+.18
    clip=np.where(t<-.72,-.72*t-.2592,np.where(t>.92,.92*t-.4232,.5*t*t))
    fold=np.where(np.abs(x)<1.2,(1-np.cos(PI*x/1.2))/(PI/1.2),2/(PI/1.2))
    return clip-.18*x+.12*fold
def adaa(x):
    old=np.r_[0.,x[:-1]];delta=x-old;y=shape(.5*(x+old));mask=np.abs(delta)>=1e-5
    y[mask]=(primitive(x[mask])-primitive(old[mask]))/delta[mask]
    return y
def residual(y,sr):
    a=np.exp(-2*PI*15/sr);y=lfilter([1,-1],[1,-a],y)
    a=np.exp(-2*PI*6600/sr);y=lfilter([1-a],[1,-a],y)
    amp=2*np.abs(np.fft.rfft(y[sr:2*sr]))/sr;mask=np.ones(len(amp),dtype=bool);mask[0]=False
    for hz in range(FREQUENCY,len(amp),FREQUENCY):mask[hz-1:hz+2]=False
    return round(float(20*np.log10(np.sqrt(np.sum(amp[mask]**2))/amp[FREQUENCY])),2)
def main():
    rows=[]
    for sr in (44100,48000,96000):
        for level in (-18,-6):
            raw=10**(level/20)*np.sin(2*PI*FREQUENCY*np.arange(sr*3)/sr)
            a=np.exp(-2*PI*1400/sr);low=lfilter([1-a],[1,-a],raw)
            driven=np.clip((raw+.6*(raw-low))*DRIVE,-100,100)
            row=dict(sample_rate=sr,input_peak_dbfs=level,adaa_residual_dbc=residual(adaa(driven),sr))
            for factor in (2,4,32):
                taps=firwin(128*factor+1,1/factor,window=('kaiser',10))
                up=resample_poly(driven,factor,1,window=taps)
                processed=resample_poly(shape(up),1,factor,window=taps)
                row[f'{factor}x_residual_dbc']=residual(processed,sr)
                del up,processed
            rows.append(row)
    print(json.dumps(dict(numpy_version=np.__version__,scipy_version=scipy.__version__,frequency_hz=FREQUENCY,fir='Kaiser beta 10; 128*factor+1 taps',rows=rows),indent=2))
if __name__=='__main__':main()
