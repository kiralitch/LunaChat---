"""生成 LunaChat 的占位提示音（16-bit PCM WAV，44.1kHz 单声道）。

只是为了"开箱能听到声音"：正式提交/答辩前建议换成自己喜欢的音频，
在设置页点"选择文件"即可替换（会覆盖 sounds/<kind>.<ext>）。

用法：python tools/make_placeholder_sounds.py [输出目录]
默认输出到 <仓库根>/runtime/sounds。
"""
import math
import os
import struct
import sys
import wave

SR = 44100


def tone(freq, seconds, volume=0.22):
    """带 8ms 淡入淡出的正弦音，避免爆音。"""
    n = int(SR * seconds)
    fade = int(SR * 0.008)
    samples = []
    for i in range(n):
        env = 1.0
        if i < fade:
            env = i / fade
        elif i > n - fade:
            env = (n - i) / fade
        samples.append(int(32767 * volume * env * math.sin(2 * math.pi * freq * i / SR)))
    return samples


def silence(seconds):
    return [0] * int(SR * seconds)


def write_wav(path, samples):
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(b"".join(struct.pack("<h", s) for s in samples))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "runtime", "sounds")
    os.makedirs(out, exist_ok=True)

    # 好友上线：两声上行，轻快但不吵
    online = tone(880, 0.12) + silence(0.04) + tone(1174, 0.16)
    # 匹配成功：三声上行，像"叮-叮-咚"
    match = tone(784, 0.11) + silence(0.03) + tone(988, 0.11) + silence(0.03) + tone(1319, 0.22)
    # 活动提醒：两声钟感，稍低沉
    activity = tone(659, 0.16) + silence(0.06) + tone(523, 0.26)

    files = {"online.wav": online, "match.wav": match, "activity.wav": activity}
    for name, samples in files.items():
        path = os.path.join(out, name)
        write_wav(path, samples)
        print("wrote", path, "%.1f KB" % (os.path.getsize(path) / 1024))


if __name__ == "__main__":
    main()
