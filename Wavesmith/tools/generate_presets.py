#!/usr/bin/env python3
"""Generates the 1000 Wavesmith factory presets (Bass, Lead, Pluck, Sequence).

All presets are created procedurally from hand-tuned sound archetypes with
seeded random variation, so the output is reproducible.

Usage: python3 generate_presets.py <output.json>
"""
import json
import random
import sys

# ---- indices (must match Source/Parameters.h) ------------------------------
T = dict(analog=0, pulse=1, vowel=2, climb=3, sync=4, fm=5, fold=6, crush=7,
         organ=8, oddeven=9, ressaw=10, noise=11, comb=12, phase=13, sqsync=14, glass=15)
DIV = {"1/64": 0, "1/32": 1, "1/16T": 2, "1/16": 3, "1/16D": 4, "1/8T": 5, "1/8": 6, "1/8D": 7,
       "1/4T": 8, "1/4": 9, "1/4D": 10, "1/2": 11, "1/1": 12, "2/1": 13, "4/1": 14}
SRC = dict(env=1, lfo1=2, lfo2=3, seq=4, vel=5, wheel=6, key=7, rand=8, at=9)
DST = dict(apos=1, bpos=2, apitch=3, bpitch=4, pitch=5, alvl=6, blvl=7, adet=8, bdet=9, fm=10,
           sub=11, noise=12, cutoff=13, reso=14, drive=15, amp=16, pan=17, lfo1rate=18, lfo2rate=19)
FLT = dict(lp12=0, lp24=1, bp=2, hp=3, notch=4)
LFO = dict(sine=0, tri=1, sawup=2, sawdown=3, square=4, sh=5, drift=6)
MODE = dict(poly=0, mono=1, legato=2)
ARP = dict(up=0, down=1, updown=2, random=3, played=4)

SAW, SQUARE, TRI, SINE = 0.666, 1.0, 0.333, 0.0


class Preset:
    def __init__(self, rng):
        self.r = rng
        self.p = {}
        self.slots = 0

    def set(self, **kw):
        self.p.update(kw)
        return self

    def osc(self, which, table, pos=0.0, uni=1, detune=0.2, level=0.75, oct=0, semi=0, fine=0.0,
            spread=0.7, pan=0.0, rand=True):
        k = "osc" + which
        self.p.update({k + "On": 1, k + "Table": T[table], k + "Pos": round(pos, 3), k + "Uni": uni,
                       k + "Detune": round(detune, 3), k + "Level": round(level, 3), k + "Oct": oct,
                       k + "Semi": semi, k + "Fine": round(fine, 1), k + "Spread": round(spread, 3),
                       k + "Pan": round(pan, 3), k + "Rand": 1 if rand else 0})
        return self

    def filt(self, ftype, cutoff, reso=0.1, env=0.0, key=0.0, drive=0.0, vel=0.0):
        self.p.update(fltOn=1, fltType=FLT[ftype], fltCutoff=round(cutoff, 1), fltReso=round(reso, 3),
                      fltEnvAmt=round(env, 3), fltKey=round(key, 3), fltDrive=round(drive, 3), fltVel=round(vel, 3))
        return self

    def amp(self, a, d, s, r):
        self.p.update(ampA=round(a, 4), ampD=round(d, 4), ampS=round(s, 3), ampR=round(r, 4))
        return self

    def env(self, a, d, s, r):
        self.p.update(envA=round(a, 4), envD=round(d, 4), envS=round(s, 3), envR=round(r, 4))
        return self

    def lfo(self, n, shape, rate=None, div=None, retrig=True):
        k = "lfo%d" % n
        self.p[k + "Shape"] = LFO[shape]
        self.p[k + "Retrig"] = 1 if retrig else 0
        if div is not None:
            self.p[k + "Sync"] = 1
            self.p[k + "Div"] = DIV[div]
        else:
            self.p[k + "Sync"] = 0
            self.p[k + "Rate"] = round(rate if rate is not None else 1.0, 3)
        return self

    def mod(self, src, dst, amt):
        if self.slots >= 6:
            return self
        i = self.slots + 1
        self.p["m%dSrc" % i] = SRC[src]
        self.p["m%dDst" % i] = DST[dst]
        self.p["m%dAmt" % i] = round(max(-1.0, min(1.0, amt)), 3)
        self.slots += 1
        return self

    def voice(self, mode="poly", glide=0.0, voices=12):
        self.p.update(polyMode=MODE[mode], glide=round(glide, 3), voices=voices)
        return self

    def dist(self, drive, mix=1.0):
        self.p.update(distOn=1, distDrive=round(drive, 3), distMix=round(mix, 3))
        return self

    def chorus(self, rate=0.6, depth=0.4, mix=0.4):
        self.p.update(chorusOn=1, chorusRate=round(rate, 3), chorusDepth=round(depth, 3), chorusMix=round(mix, 3))
        return self

    def delay(self, div="1/8D", fb=0.35, mix=0.2, tone=0.6):
        self.p.update(delayOn=1, delayDiv=DIV[div], delayFb=round(fb, 3), delayMix=round(mix, 3), delayTone=round(tone, 3))
        return self

    def reverb(self, size=0.6, mix=0.2, damp=0.5, width=1.0):
        self.p.update(reverbOn=1, reverbSize=round(size, 3), reverbMix=round(mix, 3),
                      reverbDamp=round(damp, 3), reverbWidth=round(width, 3))
        return self

    def sub(self, level, oct=0, shape=0):
        self.p.update(subLevel=round(level, 3), subOct=oct, subShape=shape)
        return self

    def noise(self, level, color=0.5):
        self.p.update(noiseLevel=round(level, 3), noiseColor=round(color, 3))
        return self

    def seq(self, steps, div="1/16", length=16, smooth=0.0):
        for i, v in enumerate(steps[:16]):
            self.p["seqStep%d" % (i + 1)] = round(max(0.0, min(1.0, v)), 3)
        self.p.update(seqDiv=DIV[div], seqLen=length, seqSmooth=round(smooth, 3))
        return self

    def arp(self, mode="up", div="1/16", octaves=1, gate=0.5):
        self.p.update(arpOn=1, arpMode=ARP[mode], arpDiv=DIV[div], arpOct=octaves, arpGate=round(gate, 3))
        return self

    def vol(self, db):
        self.p["masterVol"] = round(db, 1)
        return self


def u(rng, a, b):
    return rng.uniform(a, b)


def pick(rng, items):
    return rng.choice(items)


def lin_exp(rng, lo, hi):
    """log-uniform value"""
    import math
    return math.exp(rng.uniform(math.log(lo), math.log(hi)))


# ---- step patterns -----------------------------------------------------------
def pattern_random(rng, density=0.6):
    return [1.0 if rng.random() < density else round(rng.uniform(0.0, 0.4), 2) for _ in range(16)]


def pattern_ramp(rng):
    start = rng.choice([0, 4, 8])
    return [((i + start) % 8) / 7.0 for i in range(16)]


def pattern_euclid(rng):
    k = rng.choice([3, 5, 7, 9, 11])
    return [1.0 if (i * k) % 16 < k else 0.15 for i in range(16)]


def pattern_melodic(rng, choices):
    return [rng.choice(choices) for _ in range(16)]


def pattern_gate(rng):
    base = rng.choice([
        [1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1],
        [1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0],
        [1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1],
        [1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0],
        [1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1],
    ])
    return [float(x) for x in base]


# ---- BASS -------------------------------------------------------------------------
def bass_sub(rng):
    p = Preset(rng)
    p.osc("A", "analog", pos=u(rng, 0.0, 0.2), level=0.9).sub(u(rng, 0.0, 0.4))
    p.filt("lp24", lin_exp(rng, 250, 900), reso=u(rng, 0, 0.2), key=0.3)
    p.amp(0.002, u(rng, 0.2, 0.8), u(rng, 0.7, 1.0), u(rng, 0.05, 0.2))
    p.voice(pick(rng, ["mono", "legato"]), glide=u(rng, 0.0, 0.08))
    if rng.random() < 0.5:
        p.dist(u(rng, 0.1, 0.35), mix=u(rng, 0.3, 0.7))
    return p.vol(-4)


def bass_reese(rng):
    p = Preset(rng)
    tbl = pick(rng, ["analog", "analog", "ressaw", "comb", "oddeven"])
    p.osc("A", tbl, pos=u(rng, 0.55, 0.75) if tbl == "analog" else u(rng, 0.2, 0.7), uni=pick(rng, [4, 5, 6, 7]),
          detune=u(rng, 0.18, 0.4), spread=u(rng, 0.3, 0.8))
    p.osc("B", "analog", pos=SAW, oct=pick(rng, [-1, 0]), uni=pick(rng, [1, 2, 3]), detune=u(rng, 0.1, 0.3), level=u(rng, 0.4, 0.7))
    p.sub(u(rng, 0.3, 0.6))
    p.filt("lp24", lin_exp(rng, 500, 2200), reso=u(rng, 0.1, 0.35), key=0.2, drive=u(rng, 0.1, 0.5))
    p.lfo(1, pick(rng, ["sine", "tri"]), rate=u(rng, 0.1, 0.6))
    p.mod("lfo1", "cutoff", u(rng, 0.05, 0.2)).mod("lfo1", "apos", u(rng, 0.05, 0.2))
    p.amp(0.003, 0.5, 1.0, u(rng, 0.08, 0.25))
    p.voice(pick(rng, ["mono", "legato"]), glide=u(rng, 0.02, 0.12))
    if rng.random() < 0.6:
        p.dist(u(rng, 0.2, 0.5), mix=u(rng, 0.4, 1.0))
    if rng.random() < 0.3:
        p.chorus(u(rng, 0.2, 0.6), u(rng, 0.2, 0.5), u(rng, 0.2, 0.4))
    return p.vol(-9)


def bass_wobble(rng):
    p = Preset(rng)
    tbl = pick(rng, ["analog", "sync", "fm", "fold", "sqsync", "phase", "ressaw"])
    p.osc("A", tbl, pos=u(rng, 0.2, 0.8), uni=pick(rng, [1, 2, 3, 4]), detune=u(rng, 0.1, 0.25))
    p.osc("B", pick(rng, ["analog", "pulse"]), pos=u(rng, 0.6, 1.0), oct=-1, level=u(rng, 0.3, 0.6))
    p.sub(u(rng, 0.4, 0.7))
    p.filt("lp24", lin_exp(rng, 150, 450), reso=u(rng, 0.3, 0.6), drive=u(rng, 0.2, 0.6))
    p.lfo(1, pick(rng, ["sine", "tri", "sawdown", "sine"]), div=pick(rng, ["1/8", "1/4", "1/8T", "1/4T", "1/16"]), retrig=True)
    p.mod("lfo1", "cutoff", u(rng, 0.35, 0.7))
    if rng.random() < 0.6:
        p.mod("lfo1", "apos", u(rng, 0.2, 0.5))
    p.amp(0.002, 0.5, 1.0, 0.12)
    p.voice("mono", glide=u(rng, 0.0, 0.06))
    p.dist(u(rng, 0.3, 0.7), mix=u(rng, 0.6, 1.0))
    return p.vol(-10)


def bass_acid(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["analog", "pulse", "analog"]), pos=pick(rng, [SAW, SQUARE, u(rng, 0.3, 0.7)]))
    p.filt("lp24", lin_exp(rng, 180, 600), reso=u(rng, 0.55, 0.85), env=u(rng, 0.35, 0.7), drive=u(rng, 0.2, 0.6), vel=u(rng, 0, 0.3))
    p.env(0.001, u(rng, 0.12, 0.4), 0.0, 0.15)
    p.amp(0.001, u(rng, 0.3, 0.9), u(rng, 0.4, 0.9), u(rng, 0.05, 0.15))
    p.voice("legato", glide=u(rng, 0.04, 0.12))
    p.mod("vel", "reso", u(rng, 0.0, 0.15))
    if rng.random() < 0.7:
        p.dist(u(rng, 0.2, 0.55), mix=u(rng, 0.5, 1.0))
    if rng.random() < 0.4:
        p.delay(pick(rng, ["1/8D", "1/16D", "1/8"]), fb=u(rng, 0.2, 0.45), mix=u(rng, 0.1, 0.25))
    return p.vol(-8)


def bass_fm(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["analog", "fm", "phase"]), pos=u(rng, 0.0, 0.4))
    p.osc("B", pick(rng, ["analog", "fm"]), pos=u(rng, 0.0, 0.3), oct=pick(rng, [0, 1]), semi=pick(rng, [0, 0, 7]),
          level=u(rng, 0.0, 0.3))
    p.set(fmAmt=round(u(rng, 0.3, 0.75), 3))
    p.sub(u(rng, 0.2, 0.5))
    p.filt("lp12", lin_exp(rng, 900, 4000), reso=u(rng, 0.05, 0.3))
    p.env(0.001, u(rng, 0.15, 0.6), u(rng, 0.0, 0.3), 0.2)
    p.mod("env", "fm", u(rng, 0.2, 0.5))
    p.lfo(2, pick(rng, ["sine", "tri", "sh"]), div=pick(rng, ["1/8", "1/16", "1/4", "1/8T"]))
    p.mod("lfo2", "fm", u(rng, 0.1, 0.3))
    p.amp(0.002, 0.6, 0.9, 0.12)
    p.voice("mono", glide=u(rng, 0.0, 0.05))
    if rng.random() < 0.7:
        p.dist(u(rng, 0.2, 0.6))
    return p.vol(-9)


def bass_neuro(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["sync", "fold", "crush", "sqsync", "comb", "phase"]), pos=u(rng, 0.2, 0.6), uni=pick(rng, [1, 2, 3]),
          detune=u(rng, 0.05, 0.2))
    p.osc("B", pick(rng, ["fold", "sync", "noise", "ressaw"]), pos=u(rng, 0.2, 0.8), oct=pick(rng, [-1, 0]), level=u(rng, 0.3, 0.6))
    p.sub(u(rng, 0.4, 0.7))
    p.filt(pick(rng, ["lp24", "lp12", "bp"]), lin_exp(rng, 600, 3000), reso=u(rng, 0.2, 0.5), drive=u(rng, 0.3, 0.8))
    p.lfo(1, pick(rng, ["sh", "drift", "square"]), div=pick(rng, ["1/16", "1/8", "1/16T"]))
    p.lfo(2, pick(rng, ["sawdown", "tri", "sine"]), div=pick(rng, ["1/4", "1/2", "1/8"]))
    p.mod("lfo1", "apos", u(rng, 0.2, 0.5)).mod("lfo2", "cutoff", u(rng, 0.2, 0.5)).mod("lfo2", "bpos", u(rng, -0.5, 0.5))
    p.amp(0.002, 0.5, 1.0, 0.1)
    p.voice("mono", glide=u(rng, 0.0, 0.05))
    p.dist(u(rng, 0.4, 0.8))
    return p.vol(-11)


def bass_pluck(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["analog", "pulse", "oddeven", "ressaw"]), pos=u(rng, 0.4, 0.9), uni=pick(rng, [1, 2]), detune=0.1)
    p.sub(u(rng, 0.3, 0.6))
    p.filt("lp24", lin_exp(rng, 150, 500), reso=u(rng, 0.15, 0.45), env=u(rng, 0.3, 0.6))
    p.env(0.001, u(rng, 0.08, 0.3), 0.0, 0.1)
    p.amp(0.001, u(rng, 0.25, 0.6), 0.0, u(rng, 0.08, 0.2))
    p.voice(pick(rng, ["mono", "poly"]), glide=u(rng, 0.0, 0.04), voices=6)
    if rng.random() < 0.5:
        p.dist(u(rng, 0.15, 0.4), mix=0.6)
    return p.vol(-6)


def bass_808(rng):
    p = Preset(rng)
    p.osc("A", "analog", pos=u(rng, 0.0, 0.12), level=0.95)
    p.filt("lp12", lin_exp(rng, 2000, 8000), reso=0.0)
    p.env(0.0, u(rng, 0.04, 0.14), 0.0, 0.05)
    p.mod("env", "pitch", u(rng, 0.15, 0.45))
    p.amp(0.001, u(rng, 0.6, 2.5), u(rng, 0.0, 0.3), u(rng, 0.15, 0.5))
    p.voice("mono", glide=u(rng, 0.0, 0.15))
    p.dist(u(rng, 0.15, 0.6), mix=u(rng, 0.4, 0.9))
    return p.vol(-5)


def bass_growl(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["vowel", "vowel", "fm", "ressaw"]), pos=u(rng, 0.0, 0.5), uni=pick(rng, [1, 2, 3]), detune=0.12)
    p.osc("B", "analog", pos=SAW, oct=-1, level=u(rng, 0.3, 0.6))
    p.sub(u(rng, 0.3, 0.6))
    p.filt(pick(rng, ["bp", "lp24", "lp12"]), lin_exp(rng, 400, 1600), reso=u(rng, 0.3, 0.6), drive=u(rng, 0.3, 0.7))
    p.lfo(1, pick(rng, ["tri", "sine", "sawup"]), div=pick(rng, ["1/8", "1/4", "1/16", "1/8T"]))
    p.mod("lfo1", "apos", u(rng, 0.5, 0.9)).mod("lfo1", "cutoff", u(rng, 0.2, 0.5))
    p.amp(0.003, 0.5, 1.0, 0.12)
    p.voice("mono", glide=u(rng, 0.0, 0.06))
    p.dist(u(rng, 0.3, 0.7))
    return p.vol(-11)


# ---- LEAD -------------------------------------------------------------------------
def lead_supersaw(rng):
    p = Preset(rng)
    p.osc("A", "analog", pos=u(rng, 0.62, 0.7), uni=pick(rng, [6, 7, 8]), detune=u(rng, 0.25, 0.45), spread=u(rng, 0.7, 1.0))
    p.osc("B", pick(rng, ["analog", "ressaw", "oddeven"]), pos=u(rng, 0.5, 0.8), uni=pick(rng, [4, 5, 6]), detune=u(rng, 0.2, 0.35),
          oct=pick(rng, [0, 1, -1]), level=u(rng, 0.4, 0.7))
    p.filt("lp12", lin_exp(rng, 4000, 14000), reso=u(rng, 0.0, 0.2), env=u(rng, 0.0, 0.15), key=0.2)
    p.env(0.002, 0.6, 0.3, 0.4)
    p.amp(u(rng, 0.002, 0.03), 0.8, u(rng, 0.7, 1.0), u(rng, 0.25, 0.7))
    p.lfo(2, "sine", rate=u(rng, 4.5, 6.0))
    p.mod("lfo2", "pitch", u(rng, 0.0, 0.006))
    p.voice("poly", voices=8)
    p.chorus(u(rng, 0.3, 0.8), u(rng, 0.2, 0.5), u(rng, 0.2, 0.4))
    p.delay(pick(rng, ["1/8D", "1/4", "1/8"]), fb=u(rng, 0.25, 0.45), mix=u(rng, 0.1, 0.25))
    p.reverb(u(rng, 0.5, 0.85), mix=u(rng, 0.15, 0.3))
    return p.vol(-13)


def lead_square(rng):
    p = Preset(rng)
    p.osc("A", "pulse", pos=u(rng, 0.0, 0.5))
    p.osc("B", pick(rng, ["pulse", "analog"]), pos=u(rng, 0.3, 1.0), fine=u(rng, -12, 12), level=u(rng, 0.3, 0.6), oct=pick(rng, [0, -1]))
    p.filt("lp24", lin_exp(rng, 1500, 7000), reso=u(rng, 0.1, 0.35), env=u(rng, 0.0, 0.3))
    p.lfo(1, "tri", rate=u(rng, 0.2, 1.5))
    p.mod("lfo1", "apos", u(rng, 0.2, 0.45))
    p.lfo(2, "sine", rate=u(rng, 4.5, 6.5))
    p.mod("lfo2", "pitch", u(rng, 0.0, 0.008))
    p.env(0.002, 0.3, 0.2, 0.3)
    p.amp(0.002, 0.5, 0.9, u(rng, 0.1, 0.4))
    p.voice(pick(rng, ["legato", "mono", "poly"]), glide=u(rng, 0.03, 0.12), voices=6)
    p.delay(pick(rng, ["1/8D", "1/4D", "1/8"]), fb=u(rng, 0.2, 0.4), mix=u(rng, 0.1, 0.25))
    if rng.random() < 0.5:
        p.reverb(u(rng, 0.4, 0.7), mix=u(rng, 0.1, 0.25))
    return p.vol(-10)


def lead_sync(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["sync", "sqsync"]), pos=u(rng, 0.1, 0.4), uni=pick(rng, [1, 2, 3]), detune=u(rng, 0.08, 0.2))
    p.osc("B", "analog", pos=SAW, oct=pick(rng, [0, -1]), level=u(rng, 0.3, 0.5))
    p.filt("lp12", lin_exp(rng, 3000, 12000), reso=u(rng, 0.0, 0.25))
    p.env(0.001, u(rng, 0.3, 1.2), u(rng, 0.0, 0.4), 0.3)
    p.mod("env", "apos", u(rng, 0.3, 0.6))
    p.mod("wheel", "apos", u(rng, 0.2, 0.4))
    p.lfo(2, "sine", rate=u(rng, 4.5, 6.5))
    p.mod("lfo2", "pitch", u(rng, 0.0, 0.007))
    p.amp(0.002, 0.5, 0.9, u(rng, 0.15, 0.4))
    p.voice(pick(rng, ["legato", "mono"]), glide=u(rng, 0.03, 0.1))
    if rng.random() < 0.5:
        p.dist(u(rng, 0.15, 0.4), mix=0.5)
    p.delay(pick(rng, ["1/8D", "1/4", "1/16D"]), fb=u(rng, 0.2, 0.45), mix=u(rng, 0.12, 0.25))
    p.reverb(u(rng, 0.4, 0.7), mix=u(rng, 0.1, 0.22))
    return p.vol(-11)


def lead_fm(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["fm", "glass", "analog", "phase"]), pos=u(rng, 0.0, 0.6))
    p.osc("B", pick(rng, ["analog", "fm"]), pos=u(rng, 0.0, 0.2), oct=pick(rng, [0, 1, 2]), semi=pick(rng, [0, 0, 7, 5]), level=0.0)
    p.set(oscBLevel=0.0, fmAmt=round(u(rng, 0.2, 0.6), 3))
    p.filt("lp12", lin_exp(rng, 3000, 14000), reso=0.1)
    p.env(0.002, u(rng, 0.3, 1.0), u(rng, 0.1, 0.5), 0.4)
    p.mod("env", "fm", u(rng, 0.2, 0.5))
    p.mod("vel", "fm", u(rng, 0.0, 0.3))
    p.lfo(2, "sine", rate=u(rng, 4.5, 6.5))
    p.mod("lfo2", "pitch", u(rng, 0.0, 0.007))
    p.amp(0.003, 0.8, 0.8, u(rng, 0.2, 0.6))
    p.voice(pick(rng, ["poly", "legato"]), glide=u(rng, 0.0, 0.08), voices=8)
    p.delay(pick(rng, ["1/8D", "1/4"]), fb=u(rng, 0.25, 0.45), mix=u(rng, 0.1, 0.25))
    p.reverb(u(rng, 0.5, 0.8), mix=u(rng, 0.15, 0.3))
    return p.vol(-10)


def lead_vowel(rng):
    p = Preset(rng)
    p.osc("A", "vowel", pos=u(rng, 0.0, 0.6), uni=pick(rng, [1, 2, 3, 4]), detune=u(rng, 0.1, 0.25))
    p.osc("B", "analog", pos=u(rng, 0.6, 0.7), level=u(rng, 0.2, 0.4), fine=u(rng, -10, 10))
    p.filt("lp12", lin_exp(rng, 2500, 9000), reso=u(rng, 0.0, 0.25))
    p.lfo(1, pick(rng, ["tri", "sine", "drift"]), rate=u(rng, 0.2, 2.0))
    p.mod("lfo1", "apos", u(rng, 0.25, 0.5)).mod("wheel", "apos", u(rng, 0.2, 0.5))
    p.lfo(2, "sine", rate=u(rng, 4.5, 6.0))
    p.mod("lfo2", "pitch", u(rng, 0.0, 0.007))
    p.amp(u(rng, 0.002, 0.05), 0.8, 0.9, u(rng, 0.2, 0.5))
    p.voice(pick(rng, ["legato", "poly"]), glide=u(rng, 0.02, 0.1), voices=8)
    p.chorus(0.4, 0.3, 0.3)
    p.delay("1/8D", fb=u(rng, 0.25, 0.45), mix=u(rng, 0.1, 0.25))
    p.reverb(u(rng, 0.5, 0.8), mix=u(rng, 0.15, 0.3))
    return p.vol(-10)


def lead_scream(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["fold", "comb", "crush", "ressaw"]), pos=u(rng, 0.2, 0.7), uni=pick(rng, [2, 3, 4]), detune=u(rng, 0.1, 0.25))
    p.osc("B", "analog", pos=SAW, oct=pick(rng, [0, 1]), uni=2, detune=0.15, level=u(rng, 0.3, 0.6))
    p.filt(pick(rng, ["lp12", "bp", "lp24"]), lin_exp(rng, 1500, 7000), reso=u(rng, 0.2, 0.5), drive=u(rng, 0.3, 0.7))
    p.env(0.001, u(rng, 0.2, 0.8), u(rng, 0.2, 0.6), 0.3)
    p.mod("env", "apos", u(rng, 0.2, 0.4))
    p.mod("at", "apos", 0.3).mod("wheel", "cutoff", 0.25)
    p.lfo(2, "sine", rate=u(rng, 5.0, 7.0))
    p.mod("lfo2", "pitch", u(rng, 0.002, 0.01))
    p.amp(0.002, 0.5, 1.0, u(rng, 0.15, 0.35))
    p.voice(pick(rng, ["legato", "mono"]), glide=u(rng, 0.03, 0.1))
    p.dist(u(rng, 0.4, 0.75))
    p.delay(pick(rng, ["1/8D", "1/4"]), fb=0.3, mix=0.15)
    p.reverb(0.5, mix=0.15)
    return p.vol(-14)


def lead_soft(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["analog", "organ", "oddeven", "glass"]), pos=u(rng, 0.1, 0.5))
    p.osc("B", pick(rng, ["analog", "climb"]), pos=u(rng, 0.0, 0.4), oct=pick(rng, [0, 1]), fine=u(rng, -8, 8), level=u(rng, 0.3, 0.5))
    p.filt("lp12", lin_exp(rng, 1500, 6000), reso=u(rng, 0.0, 0.2), env=u(rng, 0.0, 0.2))
    p.env(u(rng, 0.01, 0.1), 0.6, 0.4, 0.4)
    p.lfo(2, "sine", rate=u(rng, 4.5, 6.0))
    p.mod("lfo2", "pitch", u(rng, 0.0, 0.006))
    p.amp(u(rng, 0.01, 0.12), 0.8, 0.85, u(rng, 0.3, 0.9))
    p.voice(pick(rng, ["poly", "legato"]), glide=u(rng, 0.0, 0.1), voices=8)
    p.chorus(u(rng, 0.3, 0.8), 0.35, 0.3)
    p.delay(pick(rng, ["1/4", "1/8D", "1/4D"]), fb=u(rng, 0.3, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.6, 0.9), mix=u(rng, 0.2, 0.35))
    return p.vol(-9)


# ---- PLUCK --------------------------------------------------------------------
def pluck_analog(rng):
    p = Preset(rng)
    p.osc("A", "analog", pos=u(rng, 0.5, 0.75), uni=pick(rng, [1, 2, 3, 4]), detune=u(rng, 0.1, 0.25))
    p.osc("B", pick(rng, ["analog", "pulse"]), pos=u(rng, 0.5, 1.0), oct=pick(rng, [0, 1, -1]), fine=u(rng, -8, 8), level=u(rng, 0.3, 0.6))
    p.filt("lp24", lin_exp(rng, 200, 900), reso=u(rng, 0.1, 0.4), env=u(rng, 0.45, 0.8), key=0.3, vel=u(rng, 0.1, 0.4))
    p.env(0.0, u(rng, 0.12, 0.45), 0.0, 0.3)
    p.amp(0.001, u(rng, 0.3, 0.9), 0.0, u(rng, 0.2, 0.5))
    p.voice("poly", voices=12)
    p.delay(pick(rng, ["1/8D", "1/8", "1/4", "1/16D"]), fb=u(rng, 0.25, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.5, 0.85), mix=u(rng, 0.15, 0.3))
    return p.vol(-8)


def pluck_bell(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["fm", "glass", "analog"]), pos=u(rng, 0.1, 0.6))
    p.osc("B", "analog", pos=0.0, oct=pick(rng, [1, 2]), semi=pick(rng, [0, 7, 0, 5]), level=0.0)
    p.set(oscBLevel=0.0, fmAmt=round(u(rng, 0.3, 0.7), 3))
    p.filt("lp12", lin_exp(rng, 4000, 15000), reso=0.05)
    p.env(0.0, u(rng, 0.2, 0.8), 0.0, 0.5)
    p.mod("env", "fm", u(rng, 0.3, 0.6)).mod("vel", "fm", u(rng, 0.1, 0.3))
    p.amp(0.001, u(rng, 0.8, 2.5), 0.0, u(rng, 0.6, 1.5))
    p.voice("poly", voices=12)
    p.delay(pick(rng, ["1/8D", "1/4", "1/4D"]), fb=u(rng, 0.3, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.6, 0.9), mix=u(rng, 0.2, 0.35))
    return p.vol(-9)


def pluck_wavetable(rng):
    p = Preset(rng)
    tbl = pick(rng, ["climb", "vowel", "sync", "comb", "phase", "ressaw", "oddeven", "noise", "fold"])
    p.osc("A", tbl, pos=u(rng, 0.0, 0.3), uni=pick(rng, [1, 2, 3]), detune=u(rng, 0.08, 0.2))
    p.osc("B", "analog", pos=u(rng, 0.0, 0.4), oct=pick(rng, [-1, 0]), level=u(rng, 0.2, 0.45))
    p.filt("lp12", lin_exp(rng, 1500, 8000), reso=u(rng, 0.0, 0.3), env=u(rng, 0.1, 0.4))
    p.env(0.0, u(rng, 0.15, 0.6), 0.0, 0.3)
    p.mod("env", "apos", u(rng, 0.4, 0.8))
    p.amp(0.001, u(rng, 0.3, 1.0), 0.0, u(rng, 0.25, 0.6))
    p.voice("poly", voices=12)
    p.delay(pick(rng, ["1/8D", "1/8", "1/4"]), fb=u(rng, 0.25, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.5, 0.85), mix=u(rng, 0.15, 0.3))
    return p.vol(-9)


def pluck_kalimba(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["organ", "climb", "glass"]), pos=u(rng, 0.0, 0.3))
    p.osc("B", "analog", pos=TRI, oct=pick(rng, [1, 2]), level=u(rng, 0.1, 0.3))
    p.noise(u(rng, 0.05, 0.15), color=u(rng, 0.5, 0.9))
    p.filt("lp12", lin_exp(rng, 1200, 5000), reso=0.1, env=u(rng, 0.2, 0.4), key=0.5)
    p.env(0.0, u(rng, 0.03, 0.1), 0.0, 0.05)
    p.mod("env", "noise", 0.6)
    p.amp(0.0005, u(rng, 0.4, 1.2), 0.0, u(rng, 0.3, 0.8))
    p.voice("poly", voices=12)
    p.delay(pick(rng, ["1/8D", "1/4"]), fb=u(rng, 0.2, 0.4), mix=u(rng, 0.1, 0.2))
    p.reverb(u(rng, 0.4, 0.8), mix=u(rng, 0.15, 0.3))
    return p.vol(-7)


def pluck_chord(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["analog", "ressaw", "oddeven"]), pos=u(rng, 0.5, 0.8), uni=pick(rng, [4, 5, 6, 7]),
          detune=u(rng, 0.2, 0.35), spread=0.9)
    p.osc("B", pick(rng, ["analog", "pulse"]), pos=u(rng, 0.6, 1.0), oct=1, uni=pick(rng, [2, 3]), detune=0.2, level=u(rng, 0.2, 0.4))
    p.filt("lp24", lin_exp(rng, 400, 1500), reso=u(rng, 0.05, 0.3), env=u(rng, 0.35, 0.6), key=0.3)
    p.env(0.0, u(rng, 0.2, 0.5), u(rng, 0.0, 0.1), 0.4)
    p.amp(0.002, u(rng, 0.4, 1.0), u(rng, 0.0, 0.15), u(rng, 0.3, 0.6))
    p.voice("poly", voices=16)
    p.chorus(u(rng, 0.3, 0.8), 0.35, 0.3)
    p.delay(pick(rng, ["1/8D", "1/4D", "1/4"]), fb=u(rng, 0.3, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.6, 0.9), mix=u(rng, 0.2, 0.35))
    return p.vol(-13)


def pluck_digital(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["crush", "sqsync", "pulse", "phase"]), pos=u(rng, 0.2, 0.8))
    p.osc("B", pick(rng, ["crush", "analog"]), pos=u(rng, 0.3, 0.9), oct=pick(rng, [0, 1]), semi=pick(rng, [0, 12, 7]), level=u(rng, 0.2, 0.5))
    p.filt(pick(rng, ["lp12", "bp", "hp"]), lin_exp(rng, 700, 4000), reso=u(rng, 0.2, 0.5), env=u(rng, 0.2, 0.5))
    p.env(0.0, u(rng, 0.06, 0.25), 0.0, 0.1)
    p.mod("env", "apos", u(rng, -0.5, 0.5))
    p.amp(0.0005, u(rng, 0.15, 0.45), 0.0, u(rng, 0.1, 0.3))
    p.voice("poly", voices=12)
    p.delay(pick(rng, ["1/16D", "1/8D", "1/8T"]), fb=u(rng, 0.3, 0.55), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.4, 0.8), mix=u(rng, 0.1, 0.25))
    return p.vol(-8)


# ---- SEQUENCE -------------------------------------------------------------------
def seq_arp_pluck(rng):
    p = pluck_analog(rng) if rng.random() < 0.5 else pluck_wavetable(rng)
    p.arp(pick(rng, ["up", "updown", "random", "played", "down"]), div=pick(rng, ["1/16", "1/16", "1/8", "1/16T", "1/8T"]),
          octaves=pick(rng, [1, 2, 2, 3]), gate=u(rng, 0.25, 0.7))
    if rng.random() < 0.5:
        p.lfo(1, pick(rng, ["sine", "tri"]), rate=u(rng, 0.05, 0.3), retrig=False)
        p.mod("lfo1", "cutoff", u(rng, 0.1, 0.3))
    return p


def seq_filter(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["analog", "ressaw", "pulse", "oddeven"]), pos=u(rng, 0.5, 0.9), uni=pick(rng, [1, 2, 3, 5]), detune=u(rng, 0.1, 0.3))
    p.osc("B", "analog", pos=SAW, oct=pick(rng, [-1, 0, 1]), fine=u(rng, -10, 10), level=u(rng, 0.3, 0.6))
    p.sub(u(rng, 0.0, 0.4))
    p.filt("lp24", lin_exp(rng, 150, 600), reso=u(rng, 0.3, 0.7), drive=u(rng, 0.0, 0.4))
    steps = pick(rng, [pattern_random, pattern_ramp, pattern_euclid])(rng)
    p.seq(steps, div=pick(rng, ["1/16", "1/16", "1/8", "1/16T"]), length=pick(rng, [16, 16, 8, 12]), smooth=u(rng, 0.0, 0.4))
    p.mod("seq", "cutoff", u(rng, 0.4, 0.75))
    p.amp(0.003, 0.5, 1.0, u(rng, 0.1, 0.4))
    p.voice(pick(rng, ["poly", "mono"]), voices=8)
    p.delay(pick(rng, ["1/8D", "1/16D", "1/4"]), fb=u(rng, 0.25, 0.5), mix=u(rng, 0.1, 0.25))
    if rng.random() < 0.5:
        p.reverb(u(rng, 0.4, 0.8), mix=u(rng, 0.1, 0.25))
    return p.vol(-9)


def seq_gate(rng):
    p = Preset(rng)
    p.osc("A", "analog", pos=u(rng, 0.62, 0.7), uni=pick(rng, [5, 6, 7]), detune=u(rng, 0.2, 0.4), spread=0.9)
    p.osc("B", pick(rng, ["analog", "vowel", "organ", "glass"]), pos=u(rng, 0.2, 0.7), oct=pick(rng, [0, 1]), uni=3, detune=0.2,
          level=u(rng, 0.3, 0.5))
    p.filt("lp12", lin_exp(rng, 2000, 9000), reso=u(rng, 0.0, 0.25))
    gate = pattern_gate(rng)
    p.seq(gate, div=pick(rng, ["1/16", "1/16", "1/8"]), smooth=u(rng, 0.02, 0.15))
    p.mod("seq", "amp", 1.0)
    p.mod("seq", "cutoff", u(rng, 0.0, 0.25))
    p.amp(u(rng, 0.005, 0.2), 0.5, 1.0, u(rng, 0.3, 0.8))
    p.voice("poly", voices=12)
    p.chorus(0.5, 0.35, 0.3)
    p.delay(pick(rng, ["1/8D", "1/16D"]), fb=u(rng, 0.3, 0.5), mix=u(rng, 0.15, 0.25))
    p.reverb(u(rng, 0.6, 0.9), mix=u(rng, 0.2, 0.35))
    return p.vol(-14)


def seq_acid(rng):
    p = bass_acid(rng)
    steps = pattern_random(rng, density=u(rng, 0.4, 0.7))
    p.seq(steps, div="1/16", length=pick(rng, [16, 16, 8]), smooth=u(rng, 0.0, 0.2))
    p.mod("seq", "cutoff", u(rng, 0.25, 0.5))
    p.arp(pick(rng, ["random", "played", "updown", "up"]), div="1/16", octaves=pick(rng, [1, 2]), gate=u(rng, 0.4, 0.9))
    return p


def seq_melodic(rng):
    """Step sequencer drives pitch: one held key plays a melody."""
    p = Preset(rng)
    tbl = pick(rng, ["analog", "pulse", "fm", "glass", "climb", "organ"])
    p.osc("A", tbl, pos=u(rng, 0.1, 0.8), uni=pick(rng, [1, 2]), detune=0.1)
    p.osc("B", "analog", pos=TRI, oct=-1, level=u(rng, 0.2, 0.4))
    p.filt("lp24", lin_exp(rng, 400, 1500), reso=u(rng, 0.1, 0.4), env=u(rng, 0.3, 0.6))
    # pitch = 24 * amt * step  ->  amt 0.5 gives 0..12 semitones
    scale = pick(rng, [[0, 3, 5, 7, 10, 12], [0, 2, 4, 7, 9, 12], [0, 3, 7, 12], [0, 5, 7, 12], [0, 2, 3, 7, 8, 12]])
    steps = pattern_melodic(rng, [s / 12.0 for s in scale])
    div = pick(rng, ["1/16", "1/8", "1/16"])
    p.seq(steps, div=div, length=pick(rng, [16, 8, 12, 16]))
    p.mod("seq", "pitch", 0.5)
    p.arp("up", div=div, octaves=1, gate=u(rng, 0.3, 0.7))
    p.env(0.0, u(rng, 0.1, 0.3), 0.0, 0.2)
    p.amp(0.001, u(rng, 0.2, 0.5), u(rng, 0.0, 0.3), u(rng, 0.1, 0.3))
    p.voice("mono")
    p.delay(pick(rng, ["1/8D", "1/16D", "1/4"]), fb=u(rng, 0.3, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.5, 0.8), mix=u(rng, 0.15, 0.3))
    return p.vol(-9)


def seq_bouncy(rng):
    p = Preset(rng)
    p.osc("A", pick(rng, ["vowel", "sync", "fold", "comb", "phase", "ressaw"]), pos=u(rng, 0.0, 0.4))
    p.osc("B", "analog", pos=SAW, oct=-1, level=u(rng, 0.2, 0.5))
    p.filt("lp12", lin_exp(rng, 900, 4000), reso=u(rng, 0.15, 0.45), env=u(rng, 0.2, 0.45))
    p.seq(pattern_random(rng, 0.5), div=pick(rng, ["1/16", "1/8"]), smooth=u(rng, 0.2, 0.7))
    p.mod("seq", "apos", u(rng, 0.4, 0.8))
    p.lfo(1, pick(rng, ["sine", "tri"]), div=pick(rng, ["1/1", "2/1", "1/2"]), retrig=False)
    p.mod("lfo1", "cutoff", u(rng, 0.1, 0.3))
    p.env(0.0, u(rng, 0.1, 0.25), 0.0, 0.2)
    p.amp(0.001, u(rng, 0.2, 0.4), 0.0, u(rng, 0.1, 0.3))
    p.arp(pick(rng, ["updown", "up", "random"]), div=pick(rng, ["1/16", "1/8T", "1/16T"]), octaves=pick(rng, [2, 3]), gate=u(rng, 0.3, 0.6))
    p.voice("poly", voices=8)
    p.delay(pick(rng, ["1/8D", "1/4"]), fb=u(rng, 0.3, 0.5), mix=u(rng, 0.15, 0.3))
    p.reverb(u(rng, 0.5, 0.8), mix=u(rng, 0.15, 0.3))
    return p.vol(-9)


def seq_bass(rng):
    p = bass_pluck(rng) if rng.random() < 0.5 else bass_reese(rng)
    p.seq(pattern_euclid(rng), div="1/16", smooth=u(rng, 0.0, 0.2))
    p.mod("seq", "cutoff", u(rng, 0.3, 0.55))
    p.arp(pick(rng, ["up", "played", "updown"]), div=pick(rng, ["1/16", "1/8", "1/16"]), octaves=pick(rng, [1, 2]), gate=u(rng, 0.4, 0.8))
    p.voice("mono", glide=u(rng, 0.0, 0.05))
    return p


# ---- names -------------------------------------------------------------------------
WORDS = {
    "Bass": (["Deep", "Rubber", "Heavy", "Low", "Dark", "Grit", "Thick", "Mud", "Iron", "Fat", "Warm", "Gravel", "Tar",
              "Basement", "Concrete", "Velvet", "Molten", "Granite", "Chunky", "Dusty", "Hollow", "Woolly", "Tight", "Brutal"],
             ["Sub", "Reese", "Wobble", "Growl", "Pulse", "Thump", "Rumble", "Engine", "Floor", "Bounce", "Drone", "Punch",
              "Wire", "Motor", "Tank", "Pressure", "Riser", "Snarl", "Throb", "Groove", "Hammer", "Bass", "Knock", "Roller"]),
    "Lead": (["Bright", "Neon", "Silver", "Laser", "Glass", "Solar", "Vivid", "Sharp", "Golden", "Crystal", "Electric", "Hyper",
              "Lucid", "Stellar", "Blazing", "Sweet", "Royal", "Cosmic", "Clean", "Radiant", "Fierce", "Lonely", "Soaring", "Wild"],
             ["Lead", "Saw", "Voice", "Beam", "Flare", "Comet", "Siren", "Anthem", "Horn", "Whistle", "Spark", "Arc", "Hero",
              "Line", "Melody", "Cry", "Rocket", "Signal", "Square", "Ribbon", "Shine", "Storm", "Glide", "Vapor"]),
    "Pluck": (["Tiny", "Wooden", "Glassy", "Soft", "Crisp", "Plastic", "Dewy", "Shiny", "Muted", "Frosty", "Candy", "Rainy",
               "Gentle", "Snappy", "Little", "Hollow", "Bamboo", "Pearl", "Velvet", "Toy", "Silk", "Cold", "Paper", "Amber"],
              ["Pluck", "Drop", "Bell", "Harp", "Pick", "Kalimba", "Chime", "Ping", "Tap", "Spark", "Marimba", "Keys", "Plink",
               "String", "Click", "Dot", "Bubble", "Pop", "Flick", "Mallet", "Box", "Droplet", "Twinkle", "Stab"]),
    "Sequence": (["Running", "Endless", "Looping", "Spinning", "Rolling", "Ticking", "Circular", "Pulsing", "Racing", "Steady",
                  "Hypnotic", "Morse", "Cascade", "Clockwork", "Restless", "Orbiting", "Shuffled", "Rhythmic", "Busy", "Night",
                  "Motion", "Machine", "Kinetic", "Robotic"],
                 ["Arp", "Sequence", "Gate", "Loop", "Pattern", "Train", "Motor", "Code", "Steps", "Wheel", "Cycle", "Grid",
                  "Engine", "Chain", "Pulse", "Stream", "Ladder", "Circuit", "Signal", "Spiral", "Chase", "Rush", "Drift", "Tracks"]),
}


def make_name(rng, cat, used):
    adj, noun = WORDS[cat]
    for _ in range(200):
        n = "%s %s" % (rng.choice(adj), rng.choice(noun))
        if n not in used:
            used.add(n)
            return n
    i = 2
    while True:
        n = "%s %s %d" % (rng.choice(adj), rng.choice(noun), i)
        if n not in used:
            used.add(n)
            return n
        i += 1


PLAN = [
    ("Bass", 300, [bass_sub, bass_reese, bass_wobble, bass_acid, bass_fm, bass_neuro, bass_pluck, bass_808, bass_growl]),
    ("Lead", 250, [lead_supersaw, lead_square, lead_sync, lead_fm, lead_vowel, lead_scream, lead_soft]),
    ("Pluck", 225, [pluck_analog, pluck_bell, pluck_wavetable, pluck_kalimba, pluck_chord, pluck_digital]),
    ("Sequence", 225, [seq_arp_pluck, seq_filter, seq_gate, seq_acid, seq_melodic, seq_bouncy, seq_bass]),
]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "factory_presets.json"
    rng = random.Random(20261121)
    used = set()
    presets = []
    for cat, count, makers in PLAN:
        for i in range(count):
            maker = makers[i % len(makers)]
            p = maker(rng)
            presets.append({"name": make_name(rng, cat, used), "category": cat, "params": p.p})
    # per-preset output level, measured by tests/PresetTest --calibrate
    import os
    cal_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calibration.json")
    if os.path.exists(cal_file):
        cal = json.load(open(cal_file))
        for p in presets:
            if p["name"] in cal:
                p["params"]["masterVol"] = cal[p["name"]]
    # sort alphabetically inside each category for a tidy browser
    order = {c: i for i, (c, _, _) in enumerate(PLAN)}
    presets.sort(key=lambda x: (order[x["category"]], x["name"]))
    with open(out, "w") as f:
        json.dump({"version": 1, "presets": presets}, f, separators=(",", ":"))
    print("wrote %d presets to %s" % (len(presets), out))


if __name__ == "__main__":
    main()
