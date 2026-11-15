#include "Core/AppSettings.h"

#include <QFile>
#include <QSettings>

namespace AppSettings {

const char *const kSoundMessage = "sound/message";
const char *const kSoundOnline = "sound/online";
const char *const kSoundMatch = "sound/match";
const char *const kSoundActivity = "sound/activity";

namespace {
QSettings &settings()
{
    static QSettings instance;
    return instance;
}
}

QString theme()
{
    return settings().value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString();
}

void setTheme(const QString &theme)
{
    settings().setValue(QStringLiteral("ui/theme"), theme);
}

QString loadStyleSheet(const QString &theme)
{
    const QString path = QStringLiteral(":/qss/theme_%1.qss").arg(theme);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

bool soundEnabled(const QString &key)
{
    return settings().value(key, true).toBool();
}

void setSoundEnabled(const QString &key, bool enabled)
{
    settings().setValue(key, enabled);
}

int soundVolume()
{
    return settings().value(QStringLiteral("sound/volume"), 70).toInt();
}

void setSoundVolume(int volume)
{
    settings().setValue(QStringLiteral("sound/volume"), qBound(0, volume, 100));
}

QString inputDeviceName()
{
    return settings().value(QStringLiteral("audio/inputDevice")).toString();
}

void setInputDeviceName(const QString &name)
{
    settings().setValue(QStringLiteral("audio/inputDevice"), name);
}

}