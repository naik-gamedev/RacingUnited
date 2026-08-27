"""Create seamless mono vehicle-engine loops from a decoded field recording.

This is an offline authoring tool. It does not run in the game. Input spans are
chosen from one consistent recording after inspecting its engine-order trace;
the tool removes DC, matches the loop seam with an overlap, and normalizes each
band without changing its pitch.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import wave

import numpy as np


def read_pcm(path: pathlib.Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as stream:
        channels = stream.getnchannels()
        sample_rate = stream.getframerate()
        sample_width = stream.getsampwidth()
        frames = stream.getnframes()
        encoded = stream.readframes(frames)

    if sample_width == 2:
        samples = np.frombuffer(encoded, dtype="<i2").astype(np.float64) / 32768.0
    elif sample_width == 3:
        octets = np.frombuffer(encoded, dtype=np.uint8).reshape(-1, 3)
        values = (octets[:, 0].astype(np.int32)
                  | (octets[:, 1].astype(np.int32) << 8)
                  | (octets[:, 2].astype(np.int32) << 16))
        values = np.where(values & 0x800000, values - 0x1000000, values)
        samples = values.astype(np.float64) / 8388608.0
    else:
        raise ValueError(f"Unsupported PCM sample width: {sample_width * 8} bit")

    return sample_rate, samples.reshape(-1, channels)


def write_pcm16(path: pathlib.Path, sample_rate: int, samples: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = np.round(np.clip(samples, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(sample_rate)
        stream.writeframes(encoded.tobytes())


def condition_loop(
    samples: np.ndarray,
    sample_rate: int,
    start_seconds: float,
    end_seconds: float,
    crossfade_seconds: float,
    target_rms_dbfs: float,
) -> np.ndarray:
    start = max(0, round(start_seconds * sample_rate))
    end = min(len(samples), round(end_seconds * sample_rate))
    fade = max(64, round(crossfade_seconds * sample_rate))
    if end - start <= fade * 3:
        raise ValueError("Authored loop span is too short for its seam overlap")

    source = samples[start:end].astype(np.float64, copy=True)
    source -= np.mean(source)

    # Reserve the last overlap as the continuation of the first samples. The
    # resulting first sample follows the final sample when XAudio2 wraps, while
    # the overlap progressively returns to the original beginning waveform.
    loop_length = len(source) - fade
    loop = source[:loop_length].copy()
    phase = np.linspace(0.0, math.pi * 0.5, fade, endpoint=False)
    fade_in = np.sin(phase) ** 2
    fade_out = np.cos(phase) ** 2
    loop[:fade] = source[loop_length:loop_length + fade] * fade_out + loop[:fade] * fade_in

    # A short cosine ramp suppresses any residual sample-level discontinuity
    # without introducing a silent gap at the loop boundary.
    seam = min(64, fade)
    t = np.linspace(0.0, 1.0, seam, endpoint=False)
    correction = (loop[0] - loop[-1]) * (0.5 - 0.5 * np.cos(math.pi * t))
    loop[-seam:] += correction

    rms = math.sqrt(float(np.mean(loop * loop)))
    target_rms = 10.0 ** (target_rms_dbfs / 20.0)
    gain = target_rms / max(rms, 1.0e-9)
    peak = float(np.max(np.abs(loop)))
    if peak * gain > 0.92:
        gain = 0.92 / peak
    return loop * gain


def condition_one_shot(
    samples: np.ndarray,
    sample_rate: int,
    start_seconds: float,
    end_seconds: float,
    target_rms_dbfs: float,
) -> np.ndarray:
    start = max(0, round(start_seconds * sample_rate))
    end = min(len(samples), round(end_seconds * sample_rate))
    result = samples[start:end].astype(np.float64, copy=True)
    result -= np.mean(result)
    ramp = min(len(result) // 4, round(0.020 * sample_rate))
    if ramp > 0:
        envelope = np.linspace(0.0, 1.0, ramp, endpoint=False)
        result[:ramp] *= envelope
        result[-ramp:] *= envelope[::-1]
    rms = math.sqrt(float(np.mean(result * result)))
    gain = 10.0 ** (target_rms_dbfs / 20.0) / max(rms, 1.0e-9)
    peak = float(np.max(np.abs(result)))
    if peak * gain > 0.92:
        gain = 0.92 / peak
    return result * gain


def select_channel(samples: np.ndarray, channel: int) -> np.ndarray:
    if channel < 0 or channel >= samples.shape[1]:
        raise ValueError(f"Channel {channel} is absent from {samples.shape[1]}-channel source")
    return samples[:, channel]


def select_channel_mix(samples: np.ndarray, weights: list[float]) -> np.ndarray:
    if len(weights) != samples.shape[1]:
        raise ValueError(
            f"sourceChannelMix has {len(weights)} weights for "
            f"{samples.shape[1]} source channels"
        )
    mix = np.asarray(weights, dtype=np.float64)
    if not np.all(np.isfinite(mix)) or float(np.sum(np.abs(mix))) <= 1.0e-9:
        raise ValueError("sourceChannelMix must contain finite, non-zero weights")
    mix /= float(np.sum(np.abs(mix)))
    return samples @ mix


def spectral_condition(
    samples: np.ndarray,
    sample_rate: int,
    low_shelf_db: float,
    low_shelf_end_hz: float,
    high_shelf_db: float,
    high_shelf_start_hz: float,
    high_shelf_end_hz: float,
) -> np.ndarray:
    """Apply zero-phase periodic shelves without damaging a seamless loop."""
    spectrum = np.fft.rfft(samples)
    frequencies = np.fft.rfftfreq(len(samples), 1.0 / sample_rate)

    def smoothstep(value: np.ndarray) -> np.ndarray:
        value = np.clip(value, 0.0, 1.0)
        return value * value * (3.0 - 2.0 * value)

    low = 1.0 - smoothstep(frequencies / max(low_shelf_end_hz, 1.0))
    high = smoothstep(
        (frequencies - high_shelf_start_hz)
        / max(high_shelf_end_hz - high_shelf_start_hz, 1.0)
    )
    gain_db = low_shelf_db * low + high_shelf_db * high
    spectrum *= np.power(10.0, gain_db / 20.0)
    spectrum[0] = 0.0
    return np.fft.irfft(spectrum, n=len(samples))


def normalize_peak_safe(samples: np.ndarray, target_rms_dbfs: float) -> np.ndarray:
    rms = math.sqrt(float(np.mean(samples * samples)))
    gain = 10.0 ** (target_rms_dbfs / 20.0) / max(rms, 1.0e-9)
    peak = float(np.max(np.abs(samples)))
    if peak * gain > 0.92:
        gain = 0.92 / peak
    return samples * gain


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("config", type=pathlib.Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding="utf-8"))
    root = pathlib.Path.cwd()
    output_dir = root / config["outputDirectory"]

    source_path = root / config["sourceWav"]
    sample_rate, source = read_pcm(source_path)
    if "sourceChannelMix" in config:
        source_channel = select_channel_mix(
            source, [float(value) for value in config["sourceChannelMix"]]
        )
    else:
        source_channel = select_channel(source, int(config.get("sourceChannel", 0)))
    target_rms = float(config.get("targetRmsDbfs", -18.0))
    crossfade = float(config.get("crossfadeSeconds", 0.080))

    manifest: list[dict[str, object]] = []
    for authored in config["loops"]:
        rpm = int(authored["rpm"])
        loop = condition_loop(
            source_channel,
            sample_rate,
            float(authored["startSeconds"]),
            float(authored["endSeconds"]),
            crossfade,
            target_rms,
        )
        equalization = config.get("equalization", {})
        loop = spectral_condition(
            loop,
            sample_rate,
            float(equalization.get("lowShelfDb", 0.0)),
            float(equalization.get("lowShelfEndHz", 160.0)),
            float(equalization.get("highShelfDb", 0.0)),
            float(equalization.get("highShelfStartHz", 1800.0)),
            float(equalization.get("highShelfEndHz", 6000.0)),
        )
        loop = normalize_peak_safe(loop, target_rms)
        filename = f"engine_{rpm:04d}_rpm.wav"
        write_pcm16(output_dir / filename, sample_rate, loop)
        manifest.append({"path": filename, "rpm": rpm})
        print(f"wrote {filename}: {len(loop) / sample_rate:.3f} seconds")

    startup = config.get("startup")
    if startup:
        startup_rate, startup_source = read_pcm(root / startup["sourceWav"])
        startup_channel = select_channel(startup_source, int(startup.get("sourceChannel", 0)))
        one_shot = condition_one_shot(
            startup_channel,
            startup_rate,
            float(startup["startSeconds"]),
            float(startup["endSeconds"]),
            float(startup.get("targetRmsDbfs", -18.0)),
        )
        write_pcm16(output_dir / "engine_start.wav", startup_rate, one_shot)
        print(f"wrote engine_start.wav: {len(one_shot) / startup_rate:.3f} seconds")

    (output_dir / "bank.json").write_text(
        json.dumps({"loops": manifest}, indent=2) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
