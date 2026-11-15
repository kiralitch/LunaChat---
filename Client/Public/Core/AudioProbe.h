#pragma once

#include <QStringList>

// 音频能力探测：录音设备、支持的采样率、Qt 能写出的容器与编码
// 用途：判断本机能否把 WAV 转成 Ogg/MP3，不能就按规范降级为 WAV
namespace LunaAudio {

QStringList describeCapabilities();

}