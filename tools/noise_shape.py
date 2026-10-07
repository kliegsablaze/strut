# Designs the noise shaper's taps (SHAPE in src/dsp/strut.c).
#
# The noise left by rounding should follow the threshold of hearing: low
# where the ear is keen, high where it is deaf (Terhardt 1979's formula for
# the threshold in quiet). A shaper is a monic filter, and the best such a
# filter can do is zero mean log gain (Gerzon and Craven 1989), so the target
# is the threshold, capped at 30 dB of range, less its mean. The cepstrum
# makes it minimum phase, and it is cut to nine taps.
#
#   python3 tools/noise_shape.py
import numpy as np

FS, M, TAPS, CAP = 44100.0, 8192, 9, 30.0
f = np.arange(M // 2 + 1) * FS / M
k = np.maximum(f, 20) / 1000.0
threshold = 3.64 * k**-0.8 - 6.5 * np.exp(-0.6 * (k - 3.3) ** 2) + 1e-3 * k**4

t = np.clip(threshold, None, threshold.min() + CAP)
logmag = (t - t.mean()) / 20 * np.log(10)
cep = np.fft.ifft(np.concatenate([logmag, logmag[-2:0:-1]])).real
fold = np.zeros(M)
fold[0], fold[1:M // 2], fold[M // 2] = 1, 2, 1
h = np.fft.ifft(np.exp(np.fft.fft(cep * fold))).real[:TAPS]
h /= h[0]

H = np.abs(np.fft.rfft(h, M)) ** 2
ear = 10 ** (-threshold / 10)
band = (f > 20) & (f < 20000)
print("heard %.1f dB against plain dither, power %+.1f dB" % (
    10 * np.log10(np.sum((ear * H)[band]) / np.sum(ear[band])), 10 * np.log10(H.mean())))
print("SHAPE =", ", ".join("%.5gf" % x for x in h[1:]))
