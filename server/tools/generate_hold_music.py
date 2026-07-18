#!/usr/bin/env python3
"""Generate a pleasant hold music WAV file (8-bit mono 8kHz)."""

import wave
import struct
import math
import sys

SAMPLE_RATE = 8000
DURATION_SEC = 30  # loop length

# Pleasant chord progression (frequencies in Hz)
# C major → Am → F → G → C major (classic hold music feel)
CHORDS = [
    # (freq, duration_beats)
    # C major chord tones
    ([261.63, 329.63, 392.00], 4),   # C4 E4 G4
    # A minor
    ([220.00, 261.63, 329.63], 4),   # A3 C4 E4
    # F major
    ([174.61, 220.00, 261.63], 4),   # F3 A3 C4
    # G major
    ([196.00, 246.94, 293.66], 4),   # G3 B3 D4
    # Back to C
    ([261.63, 329.63, 392.00], 4),   # C4 E4 G4
    # G7 (resolution)
    ([196.00, 246.94, 349.23], 4),   # G3 B3 F4
]

BPM = 72
BEAT_SEC = 60.0 / BPM

def generate_tone(freq, duration, sample_rate, volume=0.3):
    """Generate a sine wave with gentle attack/release envelope."""
    n_samples = int(sample_rate * duration)
    samples = []
    attack = min(int(sample_rate * 0.05), n_samples // 4)  # 50ms attack
    release = min(int(sample_rate * 0.08), n_samples // 4)  # 80ms release

    for i in range(n_samples):
        t = i / sample_rate
        # Sine wave
        val = math.sin(2 * math.pi * freq * t)
        # Add slight harmonic for warmth
        val += 0.15 * math.sin(2 * math.pi * freq * 2 * t)
        val += 0.08 * math.sin(2 * math.pi * freq * 3 * t)
        # Envelope
        env = 1.0
        if i < attack:
            env = i / attack
        elif i > n_samples - release:
            env = (n_samples - i) / release
        samples.append(val * volume * env)
    return samples

def mix_samples(*sample_lists):
    """Mix multiple sample lists together."""
    max_len = max(len(s) for s in sample_lists)
    result = [0.0] * max_len
    for samples in sample_lists:
        for i, val in enumerate(samples):
            result[i] += val
    return result

def normalize(samples, target_peak=0.85):
    """Normalize to target peak level."""
    peak = max(abs(s) for s in samples) or 1.0
    return [s * target_peak / peak for s in samples]

def main():
    output_file = sys.argv[1] if len(sys.argv) > 1 else "hold_music.wav"

    all_samples = []
    total_duration = 0

    # Generate enough to fill DURATION_SEC
    while total_duration < DURATION_SEC:
        for chord_freqs, beats in CHORDS:
            duration = beats * BEAT_SEC
            chord_samples = [generate_tone(f, duration, SAMPLE_RATE, volume=0.25) for f in chord_freqs]
            mixed = mix_samples(*chord_samples)

            # Add soft bass root
            bass = generate_tone(chord_freqs[0] / 2, duration, SAMPLE_RATE, volume=0.15)
            mixed = mix_samples(mixed, bass)

            all_samples.extend(mixed)
            total_duration += duration

    # Trim to exact length
    target_samples = int(SAMPLE_RATE * DURATION_SEC)
    all_samples = all_samples[:target_samples]

    # Normalize
    all_samples = normalize(all_samples)

    # Write WAV
    with wave.open(output_file, 'w') as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)  # 16-bit
        wav.setframerate(SAMPLE_RATE)
        for s in all_samples:
            val = max(-1.0, min(1.0, s))
            wav.writeframes(struct.pack('<h', int(val * 32767)))

    size_kb = target_samples * 2 / 1024
    print(f"Generated {output_file}: {DURATION_SEC}s, {size_kb:.0f} KB, {SAMPLE_RATE} Hz mono 16-bit")

if __name__ == '__main__':
    main()
