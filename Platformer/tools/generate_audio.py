"""生成 Platformer 的背景音乐与音效（程序化、无外部素材）。

用法：python Platformer/tools/generate_audio.py
输出：Platformer/assets/audio/ 下的 bgm.wav 与各音效 wav
格式：44.1 kHz / 16-bit / 单声道 PCM
"""

import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "audio"


def midi_to_freq(note: int) -> float:
    return 440.0 * (2.0 ** ((note - 69) / 12.0))


def write_wav(name: str, samples: list[float], target_peak: float = 0.8) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    peak = max((abs(value) for value in samples), default=0.0)
    scale = target_peak / peak if peak > target_peak else 1.0

    frames = bytearray()
    for value in samples:
        clamped = max(-1.0, min(1.0, value * scale))
        frames += struct.pack("<h", int(clamped * 32767.0))

    path = OUT_DIR / name
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(frames)
    print(f"{path.name}: {len(samples) / SAMPLE_RATE:.2f}s, {len(frames)} bytes")


def fade_edges(samples: list[float], seconds: float = 0.008) -> list[float]:
    frames = max(1, int(seconds * SAMPLE_RATE))
    result = samples[:]
    for i in range(min(frames, len(result))):
        gain = i / frames
        result[i] *= gain
        result[-1 - i] *= gain
    return result


def oscillator(waveform: str, phase: float) -> float:
    if waveform == "triangle":
        return (2.0 / math.pi) * math.asin(math.sin(phase))
    if waveform == "chip":
        return math.sin(phase) + 0.25 * math.sin(phase * 2.0) + 0.08 * math.sin(phase * 3.0)
    return math.sin(phase)


def add_note(buffer: list[float], start: float, duration: float, frequency: float, gain: float,
             waveform: str = "sine", attack: float = 0.004, release: float = 0.04,
             decay: float = 0.0) -> None:
    start_frame = int(start * SAMPLE_RATE)
    frame_count = int(duration * SAMPLE_RATE)
    phase = 0.0
    for i in range(frame_count):
        index = start_frame + i
        if index < 0 or index >= len(buffer):
            break
        t = i / SAMPLE_RATE
        attack_gain = min(1.0, t / attack) if attack > 0.0 else 1.0
        release_gain = min(1.0, (duration - t) / release) if release > 0.0 else 1.0
        envelope = attack_gain * max(0.0, release_gain)
        if decay > 0.0:
            envelope *= math.exp(-t * decay)
        buffer[index] += oscillator(waveform, phase) * envelope * gain
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE


def add_sweep(buffer: list[float], duration: float, start_freq: float, end_freq: float,
              gain: float, waveform: str = "chip", decay: float = 0.0) -> None:
    frame_count = int(duration * SAMPLE_RATE)
    phase = 0.0
    for i in range(frame_count):
        t = i / SAMPLE_RATE
        ratio = t / duration
        frequency = start_freq * ((end_freq / start_freq) ** ratio)
        attack = min(1.0, t / 0.003)
        envelope = attack * max(0.0, min(1.0, (duration - t) / 0.03))
        if decay > 0.0:
            envelope *= math.exp(-t * decay)
        buffer[i] += oscillator(waveform, phase) * envelope * gain
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE


def add_noise(buffer: list[float], duration: float, gain: float, cutoff: float,
              decay: float, rng: random.Random) -> None:
    frame_count = int(duration * SAMPLE_RATE)
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / SAMPLE_RATE)
    lowpass = 0.0
    for i in range(frame_count):
        t = i / SAMPLE_RATE
        lowpass += alpha * (rng.uniform(-1.0, 1.0) - lowpass)
        envelope = math.exp(-t * decay) * min(1.0, t / 0.004) * max(0.0, min(1.0, (duration - t) / 0.03))
        buffer[i] += lowpass * envelope * gain


def make_bgm() -> list[float]:
    bpm = 120.0
    beat = 60.0 / bpm
    bar = beat * 4.0
    bars = 8
    duration = bar * bars
    samples = [0.0] * int(duration * SAMPLE_RATE)

    # I - V - vi - IV（C - G - Am - F），循环两遍
    chords = [
        (48, 52, 55, 60),
        (43, 47, 50, 55),
        (45, 48, 52, 57),
        (41, 45, 48, 53),
    ] * 2

    for bar_index, chord in enumerate(chords):
        start = bar_index * bar

        # 铺底和弦：慢起音，尾部释放落在下一小节内，避免循环边界爆音
        for note in chord:
            add_note(samples, start, bar, midi_to_freq(note), 0.055,
                     waveform="sine", attack=0.12, release=0.30)

        # 低音：每拍一次
        for beat_index in range(4):
            add_note(samples, start + beat_index * beat, beat * 0.9,
                     midi_to_freq(chord[0] - 12), 0.13,
                     waveform="triangle", attack=0.008, release=0.05, decay=1.5)

        # 琶音：八分音符，高八度，带一点芯片音色
        pattern = [0, 1, 2, 3, 2, 1, 3, 2]
        for step, chord_index in enumerate(pattern):
            note = chord[chord_index % len(chord)] + 12
            add_note(samples, start + step * beat * 0.5, beat * 0.42,
                     midi_to_freq(note), 0.040,
                     waveform="chip", attack=0.003, release=0.04, decay=7.0)

    return fade_edges(samples, 0.010)


def make_jump() -> list[float]:
    samples = [0.0] * int(0.18 * SAMPLE_RATE)
    add_sweep(samples, 0.18, 420.0, 980.0, 0.38, waveform="chip", decay=8.0)
    return fade_edges(samples, 0.004)


def make_land(rng: random.Random) -> list[float]:
    samples = [0.0] * int(0.14 * SAMPLE_RATE)
    add_sweep(samples, 0.14, 150.0, 70.0, 0.35, waveform="triangle", decay=20.0)
    add_noise(samples, 0.14, 0.16, 900.0, 26.0, rng)
    return fade_edges(samples, 0.005)


def make_coin() -> list[float]:
    samples = [0.0] * int(0.32 * SAMPLE_RATE)
    add_note(samples, 0.00, 0.22, midi_to_freq(83), 0.30, waveform="sine", decay=10.0)
    add_note(samples, 0.07, 0.25, midi_to_freq(88), 0.26, waveform="sine", decay=9.0)
    add_note(samples, 0.07, 0.25, midi_to_freq(100), 0.12, waveform="sine", decay=12.0)
    return fade_edges(samples, 0.006)


def make_checkpoint() -> list[float]:
    samples = [0.0] * int(0.62 * SAMPLE_RATE)
    for index, note in enumerate((72, 76, 79, 84)):
        add_note(samples, index * 0.08, 0.42, midi_to_freq(note), 0.24,
                 waveform="sine", attack=0.004, release=0.08, decay=4.0)
    add_note(samples, 0.24, 0.36, midi_to_freq(96), 0.10,
             waveform="sine", attack=0.004, release=0.08, decay=5.0)
    return fade_edges(samples, 0.008)


def make_death(rng: random.Random) -> list[float]:
    samples = [0.0] * int(0.62 * SAMPLE_RATE)
    add_sweep(samples, 0.62, 330.0, 70.0, 0.42, waveform="chip", decay=3.5)
    add_sweep(samples, 0.62, 165.0, 45.0, 0.18, waveform="triangle", decay=3.0)
    add_noise(samples, 0.20, 0.12, 1400.0, 12.0, rng)
    return fade_edges(samples, 0.008)


def make_victory() -> list[float]:
    samples = [0.0] * int(1.85 * SAMPLE_RATE)
    fanfare = ((0.00, 72), (0.12, 76), (0.24, 79), (0.36, 84))
    for start, note in fanfare:
        add_note(samples, start, 0.38, midi_to_freq(note), 0.20,
                 waveform="chip", attack=0.004, release=0.08, decay=3.5)

    for note in (72, 76, 79, 84):
        add_note(samples, 0.62, 1.15, midi_to_freq(note), 0.10,
                 waveform="sine", attack=0.05, release=0.30)
    add_note(samples, 0.62, 1.10, midi_to_freq(96), 0.08,
             waveform="sine", attack=0.03, release=0.30, decay=1.2)
    return fade_edges(samples, 0.012)


def main() -> None:
    rng = random.Random(20261027)
    write_wav("bgm.wav", make_bgm(), target_peak=0.55)
    write_wav("jump.wav", make_jump(), target_peak=0.75)
    write_wav("land.wav", make_land(rng), target_peak=0.70)
    write_wav("coin.wav", make_coin(), target_peak=0.75)
    write_wav("checkpoint.wav", make_checkpoint(), target_peak=0.75)
    write_wav("death.wav", make_death(rng), target_peak=0.80)
    write_wav("victory.wav", make_victory(), target_peak=0.80)


if __name__ == "__main__":
    main()
