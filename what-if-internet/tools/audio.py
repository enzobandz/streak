"""Procedural, original audio for the episode (no samples, no licences needed).

1. Sound-effects library  -> public/sfx/<name>.wav
2. Score that follows the timeline (mood layers per scene, cross-faded at scene
   boundaries, side-chain ducked under narration) -> public/music/score.wav

Fully deterministic: all noise comes from fixed seeds.
Run after tools/tts.py (needs src/data/timeline.json).
"""
import json, os
import numpy as np
import soundfile as sf
from scipy import signal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 44100
rng = np.random.default_rng(20241114)


def t_axis(sec):
    return np.arange(int(sec * SR)) / SR


def env_adsr(n, a=0.01, d=0.1, s=0.7, r=0.2):
    e = np.ones(n) * s
    A, D, R = int(a * SR), int(d * SR), int(r * SR)
    A = min(A, n); e[:A] = np.linspace(0, 1, A)
    D = min(D, n - A); e[A:A + D] = np.linspace(1, s, D)
    if R > 0 and R < n:
        e[-R:] *= np.linspace(1, 0, R)
    return e


def lp(x, cut, order=2):
    b, a = signal.butter(order, cut / (SR / 2), 'low'); return signal.lfilter(b, a, x)


def hp(x, cut, order=2):
    b, a = signal.butter(order, cut / (SR / 2), 'high'); return signal.lfilter(b, a, x)


def bp(x, lo, hi, order=2):
    b, a = signal.butter(order, [lo / (SR / 2), hi / (SR / 2)], 'band'); return signal.lfilter(b, a, x)


def sweep_lp(x, f0, f1):
    """Time-varying one-pole low-pass (cutoff glides f0 -> f1, exponential)."""
    n = len(x)
    cuts = f0 * (f1 / f0) ** (np.arange(n) / n)
    alpha = 1 - np.exp(-2 * np.pi * cuts / SR)
    y = np.zeros(n); acc = 0.0
    for i in range(n):
        acc += alpha[i] * (x[i] - acc); y[i] = acc
    return y


def reverb_ir(sec=2.4, decay=3.2, seed=1):
    r = np.random.default_rng(seed)
    t = t_axis(sec)
    ir = r.standard_normal(len(t)) * np.exp(-decay * t)
    ir = lp(ir, 6000)
    ir[0] = 1.0
    return ir / np.sqrt(np.sum(ir ** 2))


IR_L, IR_R = reverb_ir(seed=11), reverb_ir(seed=12)


def stereo_verb(x, wet=0.25):
    l = signal.oaconvolve(x, IR_L)[:len(x)]
    r = signal.oaconvolve(x, IR_R)[:len(x)]
    return np.stack([x * (1 - wet) + l * wet, x * (1 - wet) + r * wet], axis=1)


def norm(x, peak=0.9):
    m = np.max(np.abs(x)) or 1; return x / m * peak


def save(path, x):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sf.write(path, x.astype(np.float32), SR, subtype='PCM_16')


# --------------------------------------------------------------------------- SFX
def sfx_library():
    out = {}
    # whoosh: swept filtered noise, swelling then decaying
    n = rng.standard_normal(int(0.9 * SR))
    e = np.sin(np.linspace(0, np.pi, len(n))) ** 2
    out['whoosh'] = norm(sweep_lp(n, 300, 5000) * e, 0.7)
    n = rng.standard_normal(int(0.6 * SR))
    out['whooshShort'] = norm(sweep_lp(n, 600, 7000) * np.sin(np.linspace(0, np.pi, len(n))) ** 3, 0.55)
    # boom: sub sweep + thump
    t = t_axis(2.5)
    f = 70 * np.exp(-t * 1.2) + 32
    boom = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 1.6)
    boom += lp(rng.standard_normal(len(t)), 200) * np.exp(-t * 9) * 0.6
    out['boom'] = norm(np.tanh(boom * 1.5), 0.95)
    # hit: cinematic title hit = boom + bright metallic shimmer
    shimmer = sum(np.sin(2 * np.pi * fr * t + k) * np.exp(-t * (2.5 + k)) / (k + 1) for k, fr in enumerate([523.3, 784, 1046.5, 1568, 2093]))
    noise = hp(rng.standard_normal(len(t)), 3000) * np.exp(-t * 6) * 0.4
    out['hit'] = norm(out['boom'] * 0.9 + 0.25 * shimmer + noise, 0.95)
    # riser
    t = t_axis(2.2)
    fr = 180 * (6 ** (t / t[-1]))
    ris = np.sin(2 * np.pi * np.cumsum(fr) / SR) * 0.4 + sweep_lp(rng.standard_normal(len(t)), 400, 9000) * 0.7
    out['riser'] = norm(ris * (t / t[-1]) ** 2.2, 0.7)
    # power down: descending saw + sine
    t = t_axis(1.8)
    fr = 900 * np.exp(-t * 3) + 30
    ph = 2 * np.pi * np.cumsum(fr) / SR
    pdn = 0.5 * signal.sawtooth(ph) + np.sin(ph)
    out['powerDown'] = norm(lp(pdn, 3000) * np.exp(-t * 1.1) * np.minimum(1, t * 40), 0.75)
    # power up / surge
    t = t_axis(2.0)
    fr = 40 + 600 * (t / t[-1]) ** 2
    ph = 2 * np.pi * np.cumsum(fr) / SR
    pup = 0.4 * signal.sawtooth(ph) + np.sin(ph) + 0.3 * np.sin(2 * np.pi * 50 * t) * signal.square(2 * np.pi * 8 * t)
    out['powerUp'] = norm(lp(pup, 4000) * (t / t[-1]) ** 1.5 * np.minimum(1, (t[-1] - t) * 20), 0.7)
    # glitch: seeded bit-crushed bursts
    t = t_axis(0.5)
    g = np.zeros(len(t))
    r = np.random.default_rng(5)
    for k in range(9):
        a = r.integers(0, len(t) - 2000); L = r.integers(600, 2400)
        g[a:a + L] += signal.square(2 * np.pi * r.uniform(150, 1800) * t[:L]) * r.uniform(0.3, 1)
    g = np.round(g * 4) / 4
    out['glitch'] = norm(g + hp(r.standard_normal(len(t)), 2000) * 0.3 * (g != 0), 0.6)
    # UI blip / pop / tick
    t = t_axis(0.12)
    out['blip'] = norm(np.sin(2 * np.pi * 1320 * t) * np.exp(-t * 40), 0.5)
    t = t_axis(0.16)
    fr = 500 + 900 * (t / t[-1])
    out['pop'] = norm(np.sin(2 * np.pi * np.cumsum(fr) / SR) * np.exp(-t * 30), 0.55)
    t = t_axis(0.04)
    out['tick'] = norm(hp(rng.standard_normal(len(t)), 2500) * np.exp(-t * 160), 0.5)
    # error buzz
    t = t_axis(0.5)
    bz = signal.square(2 * np.pi * 160 * t) * ((t < 0.18) | ((t > 0.26) & (t < 0.44)))
    out['error'] = norm(lp(bz, 1800), 0.45)
    # success chime
    t = t_axis(1.2)
    ch = np.sin(2 * np.pi * 880 * t) * np.exp(-t * 4) + 0.8 * np.sin(2 * np.pi * 1318.5 * t) * np.exp(-(t - 0.12).clip(0) * 4) * (t > 0.12)
    out['chime'] = norm(ch, 0.5)
    # notify ping
    t = t_axis(0.6)
    ping = np.sin(2 * np.pi * 1046.5 * t) * np.exp(-t * 9) + np.sin(2 * np.pi * 1568 * t) * np.exp(-(t - 0.09).clip(0) * 9) * (t > 0.09)
    out['ping'] = norm(ping, 0.45)
    # coin / register bell
    t = t_axis(0.9)
    coin = sum(np.sin(2 * np.pi * fr * t) * np.exp(-t * d) for fr, d in [(2637, 7), (3951, 9), (5274, 12), (1975, 5)])
    out['coin'] = norm(coin, 0.4)
    # switch clunk
    t = t_axis(0.35)
    cl = lp(rng.standard_normal(len(t)), 900) * np.exp(-t * 35) + np.sin(2 * np.pi * 110 * t) * np.exp(-t * 25)
    out['switch'] = norm(cl, 0.8)
    # radio static with crackles
    t = t_axis(2.0)
    st = bp(rng.standard_normal(len(t)), 800, 5000) * 0.4
    crack = (rng.random(len(t)) > 0.9993) * rng.standard_normal(len(t)) * 4
    out['static'] = norm((st + crack) * np.minimum(1, t * 8) * np.minimum(1, (t[-1] - t) * 4), 0.4)
    # radar / sonar ping
    t = t_axis(1.4)
    out['radar'] = norm(np.sin(2 * np.pi * 1200 * t) * np.exp(-t * 5) * 0.8 + np.sin(2 * np.pi * 1210 * t) * np.exp(-t * 5) * 0.5, 0.35)
    # jet flyby
    t = t_axis(4.5)
    jet = sweep_lp(rng.standard_normal(len(t)), 300, 1600) * np.sin(np.linspace(0, np.pi, len(t))) ** 2
    out['jet'] = norm(jet + 0.2 * np.sin(2 * np.pi * (140 - 30 * t / t[-1]) * t) * np.sin(np.linspace(0, np.pi, len(t))), 0.5)
    # city ambience (soft rumble + distant swells), 8 s
    t = t_axis(8)
    amb = lp(rng.standard_normal(len(t)), 350) * 0.6
    for k in range(4):
        c = k * 2 + 0.7
        amb += lp(rng.standard_normal(len(t)), 900) * np.exp(-((t - c) ** 2) / 0.4) * 0.5
    out['city'] = norm(amb * np.minimum(1, t) * np.minimum(1, t[-1] - t), 0.25)
    # typing / keyboard clicks
    t = t_axis(1.5); ty = np.zeros(len(t)); r = np.random.default_rng(9)
    pos = np.cumsum(r.uniform(0.06, 0.16, 20))
    for p in pos:
        i = int(p * SR)
        if i < len(t) - 2000:
            ty[i:i + 2000] += hp(r.standard_normal(2000), 1500) * np.exp(-np.arange(2000) / SR * 90)
    out['typing'] = norm(ty, 0.35)
    # hum (data centre)
    t = t_axis(3)
    out['hum'] = norm((np.sin(2 * np.pi * 60 * t) + 0.5 * np.sin(2 * np.pi * 120 * t) + 0.2 * lp(rng.standard_normal(len(t)), 500)) * np.minimum(1, t * 2) * np.minimum(1, (t[-1] - t) * 2), 0.3)
    # heartbeat-like low pulse for tension
    t = t_axis(1.0)
    hb = np.sin(2 * np.pi * 55 * t) * np.exp(-t * 14) + 0.7 * np.sin(2 * np.pi * 50 * (t - 0.25).clip(0)) * np.exp(-(t - 0.25).clip(0) * 14) * (t > 0.25)
    out['pulse'] = norm(hb, 0.7)
    for k, v in out.items():
        save(os.path.join(ROOT, f'public/sfx/{k}.wav'), np.asarray(v))
    print(f'sfx: {len(out)} effects')


# --------------------------------------------------------------------------- MUSIC
BPM = 92
BEAT = 60 / BPM
BAR = BEAT * 4

def mtof(m):
    return 440 * 2 ** ((m - 69) / 12)

# D minor: Dm  Bb  F  C   (i VI III VII)
CHORDS = [[50, 53, 57, 62], [46, 50, 53, 58], [53, 57, 60, 65], [48, 52, 55, 60]]
ROOTS = [38, 34, 41, 36]

LAYERS = ['pad', 'arp', 'bass', 'kick', 'hat', 'piano', 'drone']
MOODS = {
    'coldOpen': dict(pad=.8, arp=.55, bass=.25, drone=.25),
    'title': dict(pad=.9, drone=.3),
    'network': dict(pad=.55, arp=.8, bass=.4, hat=.25),
    'firstMinutes': dict(pad=.45, arp=.35, bass=.6, kick=.3, drone=.45),
    'money': dict(pad=.4, bass=.7, kick=.5, hat=.4, drone=.35),
    'transport': dict(pad=.4, arp=.45, bass=.6, kick=.5, hat=.5),
    'critical': dict(pad=.8, arp=.45, piano=.45, bass=.3),
    'work': dict(pad=.5, arp=.6, bass=.35, hat=.2),
    'human': dict(pad=.6, piano=.85, bass=.2),
    'restart': dict(pad=.6, arp=1.0, bass=.7, kick=.6, hat=.55),
    'outro': dict(pad=.85, piano=.7, arp=.35, bass=.3),
}


def pluck(freq, dur, bright=1.0):
    t = t_axis(dur)
    y = sum((1 / k ** 1.4) * np.sin(2 * np.pi * freq * k * t) * np.exp(-t * (3 + 2.5 * k / bright)) for k in range(1, 7))
    return y * np.minimum(1, t * 400)


def piano(freq, dur):
    t = t_axis(dur)
    y = sum((0.6 / k ** 1.1) * np.sin(2 * np.pi * freq * k * (1 + 0.0004 * k * k) * t) * np.exp(-t * (1.3 + 0.9 * k)) for k in range(1, 8))
    return y * np.minimum(1, t * 300)


def render_layers(total):
    n = int(total * SR)
    L = {k: np.zeros(n) for k in LAYERS}
    bars = int(np.ceil(total / BAR)) + 1
    # pad: cached per chord, overlapped
    pad_cache = {}
    for ci, ch in enumerate(CHORDS):
        t = t_axis(BAR + 1.2)
        y = np.zeros(len(t))
        for m in ch:
            for det in (-0.08, 0, 0.07):
                y += signal.sawtooth(2 * np.pi * mtof(m + det) * t + det * 10)
        y = lp(y, 1400, 2) * 0.08
        e = np.minimum(1, t / 0.6) * np.minimum(1, (t[-1] - t) / 1.2)
        pad_cache[ci] = y * e
    for b in range(bars):
        ci = b % 4; a = int(b * BAR * SR); seg = pad_cache[ci]
        e = min(len(seg), n - a)
        if e > 0: L['pad'][a:a + e] += seg[:e]
    # arp: 16ths over chord tones (+octave)
    pattern = [0, 2, 1, 3, 2, 1, 3, 2]
    for b in range(bars):
        ch = CHORDS[b % 4]
        for s in range(8):
            a = int((b * BAR + s * BEAT / 2) * SR)
            if a >= n: break
            note = ch[pattern[s] % 4] + 12
            y = pluck(mtof(note), 0.9) * (0.9 if s % 2 == 0 else 0.6)
            e = min(len(y), n - a); L['arp'][a:a + e] += y[:e] * 0.25
    # bass: 8th-note pulse on root
    for b in range(bars):
        f0 = mtof(ROOTS[b % 4])
        for s in range(8):
            a = int((b * BAR + s * BEAT / 2) * SR)
            if a >= n: break
            t = t_axis(BEAT / 2 * 0.95)
            y = (np.sin(2 * np.pi * f0 * t) + 0.35 * np.tanh(3 * np.sin(2 * np.pi * f0 * t))) * np.exp(-t * 5) * np.minimum(1, t * 200)
            e = min(len(y), n - a); L['bass'][a:a + e] += y[:e] * (0.5 if s % 2 == 0 else 0.35)
    # kick on 1 & 3, hat on off-beats
    tk = t_axis(0.45)
    kick = np.sin(2 * np.pi * np.cumsum(50 + 110 * np.exp(-tk * 30)) / SR) * np.exp(-tk * 8)
    hr = np.random.default_rng(77)
    hat = hp(hr.standard_normal(int(0.08 * SR)), 7000) * np.exp(-t_axis(0.08) * 60)
    for b in range(bars):
        for beat in range(4):
            a = int((b * BAR + beat * BEAT) * SR)
            if a >= n: break
            if beat in (0, 2):
                e = min(len(kick), n - a); L['kick'][a:a + e] += kick[:e] * 0.7
            a2 = int((b * BAR + beat * BEAT + BEAT / 2) * SR)
            if a2 < n:
                e = min(len(hat), n - a2); L['hat'][a2:a2 + e] += hat[:e] * 0.25
    # piano: gentle broken chord every bar
    for b in range(bars):
        ch = CHORDS[b % 4]
        for k, m in enumerate([ch[0], ch[2], ch[3] + 12, ch[1] + 12]):
            a = int((b * BAR + k * BEAT) * SR)
            if a >= n: break
            y = piano(mtof(m + 12 if k > 1 else m + 12), 2.4) * 0.22
            e = min(len(y), n - a); L['piano'][a:a + e] += y[:e]
    # drone: low D with slow beating
    t = np.arange(n) / SR
    L['drone'] = lp(signal.sawtooth(2 * np.pi * mtof(26) * t) + signal.sawtooth(2 * np.pi * mtof(26.1) * t), 300) * 0.12 * (0.7 + 0.3 * np.sin(2 * np.pi * 0.1 * t))
    return L


def smooth_steps(points, n, xfade):
    """points: list of (start_sample, value). Piecewise-constant with cosine cross-fades."""
    out = np.zeros(n)
    for i, (a, v) in enumerate(points):
        b = points[i + 1][0] if i + 1 < len(points) else n
        out[a:b] = v
    k = int(xfade * SR)
    if k > 1:
        win = np.hanning(k); win /= win.sum()
        out = signal.oaconvolve(out, win, mode='same')
    return out


def score():
    tl = json.load(open(os.path.join(ROOT, 'src/data/timeline.json')))
    fps = tl['fps']
    total = tl['durationInFrames'] / fps
    n = int(total * SR)
    L = render_layers(total)
    mix = np.zeros(n)
    for layer in LAYERS:
        pts = [(int(sc['from'] / fps * SR), MOODS.get(sc['id'], {}).get(layer, 0.0)) for sc in tl['scenes']]
        g = smooth_steps(pts, n, 1.6)
        mix += L[layer][:n] * g
    # cold-open blackout: music cuts out as the network dies ("...simply stopped")
    co = tl['scenes'][0]
    c3 = [l for l in co['lines'] if l['id'] == 'c3'][0]
    cut = int((co['from'] + c3['from'] + c3['pauses'][-1] - 6) / fps * SR)  # same frame as the visual shutdown
    title_start = int(tl['scenes'][1]['from'] / fps * SR)
    blk = np.ones(n)
    fall = int(0.35 * SR)
    blk[cut:cut + fall] = np.linspace(1, 0, fall)
    blk[cut + fall:title_start] = 0
    mix *= blk
    # end fade
    endf = int(4 * SR); mix[-endf:] *= np.linspace(1, 0, endf) ** 1.5
    # side-chain ducking under narration
    vo = np.zeros(n)
    for sc in tl['scenes']:
        for ln in sc['lines']:
            a = int((sc['from'] + ln['from']) / fps * SR); b = int((sc['from'] + ln['from'] + ln['duration']) / fps * SR)
            vo[a:b] = 1
    att = np.hanning(int(0.6 * SR)); att /= att.sum()
    vo = np.clip(signal.oaconvolve(vo, att, mode='same') * 1.4, 0, 1)
    mix *= 1 - 0.5 * vo
    st = stereo_verb(mix, wet=0.3)
    # gentle stereo widening via short delay on the right channel
    d = int(0.011 * SR); st[d:, 1] = 0.85 * st[d:, 1] + 0.15 * st[:-d, 0]
    st = np.tanh(st * 1.2) / np.tanh(1.2)
    st = st / (np.max(np.abs(st)) or 1) * 0.5
    wav = os.path.join(ROOT, 'public/music/score.wav')
    save(wav, st)
    import subprocess
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', wav, '-c:a', 'libmp3lame', '-b:a', '192k', wav[:-4] + '.mp3'], check=True)
    print(f'score: {total:.1f}s')


if __name__ == '__main__':
    sfx_library()
    score()
