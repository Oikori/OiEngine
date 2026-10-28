"""生成 Survivors 的背景音乐与音效（程序化、无外部素材）。

用法：python Survivors/tools/generate_audio.py
输出：Survivors/assets/audio/ 下的 bgm.wav / bgm_boss.wav 与各音效 wav
格式：44.1 kHz / 16-bit / 单声道 PCM

风格：D 小调暗夜氛围循环 + 更快的 BOSS 段；音效偏“像素 + 噪点”，短促不刺耳。
"""

import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "audio"
TAU = 2.0 * math.pi


def midi_to_freq(note: int) -> float:
    return 440.0 * (2.0 ** ((note - 69) / 12.0))


def write_wav(name: str, samples: list[float], target_peak: float = 0.8) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    # 统一归一化到 target_peak：让每段素材的响度基准一致，混音层级只靠 SFX_VOLUME 调
    peak = max((abs(value) for value in samples), default=0.0)
    scale = target_peak / peak if peak > 0.0001 else 1.0

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
    if waveform == "saw":
        wrapped = (phase / TAU) % 1.0
        return 2.0 * wrapped - 1.0
    if waveform == "pulse":
        return 1.0 if math.sin(phase) >= 0.0 else -1.0
    return math.sin(phase)


def add_note(buffer: list[float], start: float, duration: float, frequency: float, gain: float,
             waveform: str = "sine", attack: float = 0.004, release: float = 0.04,
             decay: float = 0.0, vibrato: float = 0.0) -> None:
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
        wobble = 1.0 + vibrato * math.sin(TAU * 5.5 * t) if vibrato > 0.0 else 1.0
        buffer[index] += oscillator(waveform, phase) * envelope * gain
        phase += TAU * frequency * wobble / SAMPLE_RATE


def add_sweep(buffer: list[float], start: float, duration: float, start_freq: float, end_freq: float,
              gain: float, waveform: str = "chip", decay: float = 0.0) -> None:
    start_frame = int(start * SAMPLE_RATE)
    frame_count = int(duration * SAMPLE_RATE)
    phase = 0.0
    for i in range(frame_count):
        index = start_frame + i
        if index < 0 or index >= len(buffer):
            break
        t = i / SAMPLE_RATE
        ratio = min(1.0, t / duration)
        frequency = start_freq * ((end_freq / start_freq) ** ratio)
        attack = min(1.0, t / 0.003)
        envelope = attack * max(0.0, min(1.0, (duration - t) / 0.03))
        if decay > 0.0:
            envelope *= math.exp(-t * decay)
        buffer[index] += oscillator(waveform, phase) * envelope * gain
        phase += TAU * frequency / SAMPLE_RATE


def add_noise(buffer: list[float], start: float, duration: float, gain: float, cutoff: float,
              decay: float, rng: random.Random, highpass: bool = False) -> None:
    start_frame = int(start * SAMPLE_RATE)
    frame_count = int(duration * SAMPLE_RATE)
    alpha = 1.0 - math.exp(-TAU * cutoff / SAMPLE_RATE)
    lowpass = 0.0
    previous = 0.0
    for i in range(frame_count):
        index = start_frame + i
        if index < 0 or index >= len(buffer):
            break
        t = i / SAMPLE_RATE
        white = rng.uniform(-1.0, 1.0)
        lowpass += alpha * (white - lowpass)
        value = (white - lowpass) if highpass else lowpass
        previous = value
        envelope = math.exp(-t * decay) * min(1.0, t / 0.004) * max(0.0, min(1.0, (duration - t) / 0.02))
        buffer[index] += previous * envelope * gain


def add_kick(buffer: list[float], start: float, gain: float = 0.20) -> None:
    # 低频心跳：55 Hz 下滑到 40 Hz，短促
    add_sweep(buffer, start, 0.16, 55.0, 40.0, gain, waveform="sine", decay=16.0)


def add_hat(buffer: list[float], start: float, gain: float, rng: random.Random) -> None:
    add_noise(buffer, start, 0.05, gain, 7000.0, 70.0, rng, highpass=True)


# ---------------------------------------------------------------- 音乐

def make_bgm() -> list[float]:
    """D 小调暗夜循环：96 BPM，8 小节 ≈ 20 秒。"""
    bpm = 96.0
    beat = 60.0 / bpm
    bar = beat * 4.0
    bars = 8
    duration = bar * bars
    samples = [0.0] * int(duration * SAMPLE_RATE)
    rng = random.Random(20261028)

    # i - VI - III - VII（Dm - Bb - F - C），循环两遍
    chords = [
        (50, 53, 57),
        (46, 50, 53),
        (41, 45, 48),
        (48, 52, 55),
    ] * 2

    for bar_index, chord in enumerate(chords):
        start = bar_index * bar
        root = chord[0]

        # 铺底：慢起音的锯齿 Pad，让整段有一点金属感
        for note in chord:
            add_note(samples, start, bar, midi_to_freq(note), 0.030,
                     waveform="saw", attack=0.35, release=0.45)
            add_note(samples, start, bar, midi_to_freq(note), 0.022,
                     waveform="sine", attack=0.30, release=0.40)

        # 低音：八分音符，切分落在后半拍
        for step in range(8):
            note = root - 12 + (12 if step in (3, 6) else 0)
            add_note(samples, start + step * beat * 0.5, beat * 0.42, midi_to_freq(note), 0.115,
                     waveform="triangle", attack=0.006, release=0.05, decay=3.2)

        # 琶音：八分音符跑和弦音 + 高八度，芯片音色
        pattern = [0, 2, 1, 2, 0, 1, 2, 1]
        for step, index in enumerate(pattern):
            note = chord[index] + 12
            add_note(samples, start + step * beat * 0.5, beat * 0.40, midi_to_freq(note), 0.033,
                     waveform="chip", attack=0.003, release=0.05, decay=8.0)

        # 心跳与踩镲
        for beat_index in range(4):
            add_kick(samples, start + beat_index * beat, 0.16 if beat_index % 2 == 0 else 0.10)
            add_hat(samples, start + beat_index * beat + beat * 0.5, 0.030, rng)

    # 稀疏旋律动机：每两小节一句，D 自然小调
    motif = ((0.0, 74), (0.75, 77), (1.5, 81), (2.5, 79))
    for phrase in range(4):
        base = phrase * bar * 2
        for offset, note in motif:
            add_note(samples, base + offset * beat, beat * 0.9, midi_to_freq(note), 0.045,
                     waveform="sine", attack=0.02, release=0.25, decay=1.8, vibrato=0.004)

    return fade_edges(samples, 0.012)


def make_bgm_boss() -> list[float]:
    """BOSS 段：138 BPM，8 小节 ≈ 14 秒，E 弗里几亚的固定低音 + 三全音。"""
    bpm = 138.0
    beat = 60.0 / bpm
    bar = beat * 4.0
    bars = 8
    duration = bar * bars
    samples = [0.0] * int(duration * SAMPLE_RATE)
    rng = random.Random(20261029)

    for bar_index in range(bars):
        start = bar_index * bar
        accent = bar_index % 2 == 1

        # 固定低音：十六分音符 E2 / F2 交替，重音落在第 1、3 拍
        for step in range(16):
            note = 40 if step % 4 else 41
            if accent and step in (6, 14):
                note = 47
            gain = 0.105 if step % 4 == 0 else 0.070
            add_note(samples, start + step * beat * 0.25, beat * 0.22, midi_to_freq(note), gain,
                     waveform="triangle", attack=0.003, release=0.03, decay=9.0)

        # 三全音长音（E3 + Bb3）：制造不安
        add_note(samples, start, bar, midi_to_freq(52), 0.028, waveform="saw",
                 attack=0.25, release=0.30)
        add_note(samples, start, bar, midi_to_freq(58), 0.024, waveform="saw",
                 attack=0.25, release=0.30)

        # 和弦刺：每小节 2、4 拍
        for hit_beat in (1, 3):
            for note in (52, 55, 58):
                add_note(samples, start + hit_beat * beat, beat * 0.35, midi_to_freq(note), 0.045,
                         waveform="chip", attack=0.004, release=0.08, decay=7.0)

        # 鼓：底鼓 1、3 拍，军鼓 2、4 拍，八分踩镲
        add_kick(samples, start, 0.22)
        add_kick(samples, start + beat * 2.0, 0.18)
        for snare_beat in (1, 3):
            add_noise(samples, start + snare_beat * beat, 0.16, 0.10, 2400.0, 22.0, rng)
        for step in range(8):
            add_hat(samples, start + step * beat * 0.5, 0.028, rng)

        # 每两小节一声警报式扫频
        if bar_index % 2 == 0:
            add_sweep(samples, start + beat * 2.0, beat * 0.9, 420.0, 600.0, 0.030,
                      waveform="chip", decay=2.0)

    return fade_edges(samples, 0.010)


# ---------------------------------------------------------------- 音效

def make_blade(rng: random.Random) -> list[float]:
    """月刃：短促的空气切割声。"""
    samples = [0.0] * int(0.20 * SAMPLE_RATE)
    add_sweep(samples, 0.0, 0.14, 1800.0, 820.0, 0.22, waveform="chip", decay=16.0)
    add_noise(samples, 0.0, 0.13, 0.10, 5200.0, 24.0, rng, highpass=True)
    return fade_edges(samples, 0.004)


def make_storm(rng: random.Random) -> list[float]:
    """雷暴：劈裂噪声 + 低频轰隆。"""
    samples = [0.0] * int(0.55 * SAMPLE_RATE)
    add_noise(samples, 0.0, 0.50, 0.32, 2600.0, 6.0, rng)
    add_noise(samples, 0.01, 0.14, 0.14, 8000.0, 30.0, rng, highpass=True)
    add_sweep(samples, 0.0, 0.42, 90.0, 46.0, 0.26, waveform="sine", decay=5.0)
    return fade_edges(samples, 0.005)


def make_hit(rng: random.Random) -> list[float]:
    """命中：极短的点状冲击，靠节流 + 变调避免糊成一片。"""
    samples = [0.0] * int(0.09 * SAMPLE_RATE)
    add_sweep(samples, 0.0, 0.07, 260.0, 150.0, 0.22, waveform="triangle", decay=40.0)
    add_noise(samples, 0.0, 0.05, 0.10, 3200.0, 55.0, rng)
    return fade_edges(samples, 0.003)


def make_kill(rng: random.Random) -> list[float]:
    """击杀：碎裂 + 下坠。"""
    samples = [0.0] * int(0.26 * SAMPLE_RATE)
    add_sweep(samples, 0.0, 0.20, 340.0, 90.0, 0.20, waveform="chip", decay=11.0)
    add_noise(samples, 0.0, 0.22, 0.12, 1800.0, 16.0, rng)
    return fade_edges(samples, 0.004)


def make_pickup() -> list[float]:
    """拾取宝石：两段小亮音。"""
    samples = [0.0] * int(0.22 * SAMPLE_RATE)
    add_note(samples, 0.00, 0.10, midi_to_freq(93), 0.20, waveform="sine", decay=22.0)
    add_note(samples, 0.06, 0.14, midi_to_freq(100), 0.16, waveform="sine", decay=20.0)
    return fade_edges(samples, 0.004)


def make_levelup() -> list[float]:
    """升级：上行三连音 + 尾音。"""
    samples = [0.0] * int(0.72 * SAMPLE_RATE)
    for index, note in enumerate((72, 76, 79)):
        add_note(samples, index * 0.075, 0.34, midi_to_freq(note), 0.20,
                 waveform="chip", attack=0.004, release=0.10, decay=4.5)
    add_note(samples, 0.22, 0.48, midi_to_freq(84), 0.16,
             waveform="sine", attack=0.006, release=0.20, decay=3.0)
    add_note(samples, 0.22, 0.48, midi_to_freq(91), 0.08,
             waveform="sine", attack=0.006, release=0.22, decay=3.4)
    return fade_edges(samples, 0.006)


def make_hurt(rng: random.Random) -> list[float]:
    """受伤：短促下坠 + 一点噪声。"""
    samples = [0.0] * int(0.34 * SAMPLE_RATE)
    add_sweep(samples, 0.0, 0.30, 420.0, 130.0, 0.30, waveform="chip", decay=6.0)
    add_sweep(samples, 0.0, 0.26, 210.0, 70.0, 0.16, waveform="triangle", decay=5.0)
    add_noise(samples, 0.0, 0.16, 0.10, 1600.0, 18.0, rng)
    return fade_edges(samples, 0.005)


def make_stage_clear() -> list[float]:
    """过关：四音上行 + 轻颤尾音。"""
    samples = [0.0] * int(1.05 * SAMPLE_RATE)
    for index, note in enumerate((69, 72, 76, 81)):
        add_note(samples, index * 0.10, 0.42, midi_to_freq(note), 0.18,
                 waveform="chip", attack=0.004, release=0.12, decay=3.6)
    add_note(samples, 0.42, 0.60, midi_to_freq(88), 0.10,
             waveform="sine", attack=0.02, release=0.30, decay=1.8, vibrato=0.003)
    return fade_edges(samples, 0.008)


def make_boss_spawn(rng: random.Random) -> list[float]:
    """Boss 登场：低频轰鸣 + 三全音钟声。"""
    samples = [0.0] * int(1.55 * SAMPLE_RATE)
    add_sweep(samples, 0.0, 1.45, 48.0, 78.0, 0.30, waveform="sine", decay=1.6)
    add_noise(samples, 0.0, 1.30, 0.16, 700.0, 2.2, rng)
    for index, note in enumerate((52, 58)):
        add_note(samples, 0.18 + index * 0.04, 1.20, midi_to_freq(note), 0.10,
                 waveform="sine", attack=0.02, release=0.40, decay=2.0)
    add_sweep(samples, 0.55, 0.55, 300.0, 900.0, 0.05, waveform="chip", decay=3.0)
    return fade_edges(samples, 0.008)


def make_victory() -> list[float]:
    """通关：D 大调小号角。"""
    samples = [0.0] * int(2.10 * SAMPLE_RATE)
    fanfare = ((0.00, 74), (0.14, 78), (0.28, 81), (0.42, 86))
    for start, note in fanfare:
        add_note(samples, start, 0.42, midi_to_freq(note), 0.18,
                 waveform="chip", attack=0.004, release=0.10, decay=3.0)
    for note in (74, 78, 81, 86):
        add_note(samples, 0.70, 1.30, midi_to_freq(note), 0.10,
                 waveform="sine", attack=0.05, release=0.40)
    add_note(samples, 0.70, 1.20, midi_to_freq(98), 0.07,
             waveform="sine", attack=0.04, release=0.40, decay=1.2)
    return fade_edges(samples, 0.012)


def make_gameover(rng: random.Random) -> list[float]:
    """阵亡：下行小调 + 噪声收尾。"""
    samples = [0.0] * int(1.60 * SAMPLE_RATE)
    for index, note in enumerate((69, 65, 62, 57)):
        add_note(samples, index * 0.22, 0.70, midi_to_freq(note), 0.16,
                 waveform="chip", attack=0.006, release=0.22, decay=2.2)
    add_sweep(samples, 0.20, 1.20, 120.0, 45.0, 0.20, waveform="sine", decay=2.4)
    add_noise(samples, 0.55, 0.85, 0.10, 900.0, 3.0, rng)
    return fade_edges(samples, 0.010)


def main() -> None:
    rng = random.Random(20261028)
    write_wav("bgm.wav", make_bgm(), target_peak=0.50)
    write_wav("bgm_boss.wav", make_bgm_boss(), target_peak=0.52)
    write_wav("blade.wav", make_blade(rng), target_peak=0.70)
    write_wav("storm.wav", make_storm(rng), target_peak=0.78)
    write_wav("hit.wav", make_hit(rng), target_peak=0.60)
    write_wav("kill.wav", make_kill(rng), target_peak=0.72)
    write_wav("pickup.wav", make_pickup(), target_peak=0.66)
    write_wav("levelup.wav", make_levelup(), target_peak=0.78)
    write_wav("hurt.wav", make_hurt(rng), target_peak=0.82)
    write_wav("stage_clear.wav", make_stage_clear(), target_peak=0.78)
    write_wav("boss_spawn.wav", make_boss_spawn(rng), target_peak=0.85)
    write_wav("victory.wav", make_victory(), target_peak=0.82)
    write_wav("gameover.wav", make_gameover(rng), target_peak=0.80)


if __name__ == "__main__":
    main()
