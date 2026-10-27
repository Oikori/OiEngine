"""生成 PlaneGame 使用的程序化音效（无外部素材、可重复构建）。

用法：python SandBox/tools/generate_audio.py
输出：SandBox/assets/audio/ 下的 laser.wav / explosion.wav / pickup.wav / hit.wav
格式：44.1 kHz / 16-bit / 单声道 PCM
"""

import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "audio"


def write_wav(name: str, samples: list[float]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / name
    frames = bytearray()
    for value in samples:
        clamped = max(-1.0, min(1.0, value))
        frames += struct.pack("<h", int(clamped * 32767.0))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(frames)
    print(f"{path.name}: {len(samples) / SAMPLE_RATE:.2f}s, {len(frames)} bytes")


def fade_edges(samples: list[float], attack: float = 0.004, release: float = 0.02) -> list[float]:
    attack_frames = max(1, int(attack * SAMPLE_RATE))
    release_frames = max(1, int(release * SAMPLE_RATE))
    result = samples[:]
    for i in range(min(attack_frames, len(result))):
        result[i] *= i / attack_frames
    for i in range(min(release_frames, len(result))):
        result[-1 - i] *= i / release_frames
    return result


def make_laser() -> list[float]:
    duration = 0.18
    count = int(duration * SAMPLE_RATE)
    samples = []
    phase = 0.0
    for i in range(count):
        t = i / SAMPLE_RATE
        freq = 1800.0 * math.pow(220.0 / 1800.0, t / duration)
        phase += 2.0 * math.pi * freq / SAMPLE_RATE
        envelope = math.exp(-t * 16.0) * min(1.0, t / 0.004)
        value = math.sin(phase) + 0.30 * math.sin(phase * 2.0) + 0.12 * math.sin(phase * 3.0)
        samples.append(value * envelope * 0.38)
    return fade_edges(samples, 0.002, 0.01)


def make_explosion(rng: random.Random) -> list[float]:
    duration = 0.85
    count = int(duration * SAMPLE_RATE)
    samples = []
    lowpass = 0.0
    thump_phase = 0.0
    for i in range(count):
        t = i / SAMPLE_RATE
        # 噪声经过时变低通：爆炸开头明亮，尾部闷
        cutoff = 5000.0 * math.exp(-t * 3.0) + 180.0
        alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / SAMPLE_RATE)
        lowpass += alpha * (rng.uniform(-1.0, 1.0) - lowpass)
        envelope = math.exp(-t * 4.6) * min(1.0, t / 0.01)
        noise = lowpass * envelope * 0.95

        # 低频冲击：音高从 95 Hz 滑向 42 Hz
        thump_freq = 95.0 * math.exp(-t * 2.5) + 42.0
        thump_phase += 2.0 * math.pi * thump_freq / SAMPLE_RATE
        thump = math.sin(thump_phase) * math.exp(-t * 7.5) * 0.55

        value = noise + thump
        samples.append(math.tanh(value * 1.6) * 0.62)
    return fade_edges(samples, 0.002, 0.05)


def make_pickup() -> list[float]:
    duration = 0.45
    count = int(duration * SAMPLE_RATE)
    samples = []
    notes = ((0.0, 880.0, 0.38), (0.085, 1318.5, 0.32), (0.17, 1760.0, 0.22))
    for i in range(count):
        t = i / SAMPLE_RATE
        value = 0.0
        for start, freq, gain in notes:
            if t >= start:
                local = t - start
                envelope = math.exp(-local * 9.0) * min(1.0, local / 0.004)
                value += math.sin(2.0 * math.pi * freq * local) * envelope * gain
                value += 0.16 * math.sin(2.0 * math.pi * freq * 2.0 * local) * envelope * gain
        samples.append(value * 0.55)
    return fade_edges(samples, 0.002, 0.06)


def make_hit(rng: random.Random) -> list[float]:
    duration = 0.45
    count = int(duration * SAMPLE_RATE)
    samples = []
    phase = 0.0
    lowpass = 0.0
    for i in range(count):
        t = i / SAMPLE_RATE
        freq = 220.0 * math.exp(-t * 4.0) + 55.0
        phase += 2.0 * math.pi * freq / SAMPLE_RATE
        # 方波感的低音 + 一点噪声颗粒
        square = 1.0 if math.sin(phase) >= 0.0 else -1.0
        lowpass += (1.0 - math.exp(-2.0 * math.pi * 900.0 / SAMPLE_RATE)) * (rng.uniform(-1.0, 1.0) - lowpass)
        envelope = math.exp(-t * 8.0) * min(1.0, t / 0.003)
        ping = math.sin(2.0 * math.pi * 760.0 * t) * math.exp(-t * 24.0) * 0.22
        value = square * 0.32 * envelope + lowpass * 0.35 * envelope + ping
        samples.append(math.tanh(value * 1.3) * 0.55)
    return fade_edges(samples, 0.002, 0.04)


def main() -> None:
    rng = random.Random(20261027)
    write_wav("laser.wav", make_laser())
    write_wav("explosion.wav", make_explosion(rng))
    write_wav("pickup.wav", make_pickup())
    write_wav("hit.wav", make_hit(rng))


if __name__ == "__main__":
    main()
