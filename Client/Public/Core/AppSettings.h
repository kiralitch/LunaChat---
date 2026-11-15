#pragma once

#include <QString>

// 全部偏好统一走 QSettings（注册表），键名集中在这里，避免界面里散落字符串
namespace AppSettings {

QString theme();                       // "dark" / "light"
void setTheme(const QString &theme);
// 从 :/qss/ 读样式表；资源缺失时返回空串（界面退回 Qt 默认样式，仍可运行）
QString loadStyleSheet(const QString &theme);

bool soundEnabled(const QString &key); // 提示音开关
void setSoundEnabled(const QString &key, bool enabled);

int soundVolume();                     // 0-100
void setSoundVolume(int volume);

QString inputDeviceName();             // 录音设备名
void setInputDeviceName(const QString &name);

// 音效键
extern const char *const kSoundMessage;   // 消息提示音
extern const char *const kSoundOnline;    // 好友上线音
extern const char *const kSoundMatch;     // 匹配成功音
extern const char *const kSoundActivity;  // 活动提醒音

}