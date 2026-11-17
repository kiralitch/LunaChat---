#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMediaPlayer>
#include <QUrl>

#include "Core/AppSettings.h"

// 统一播放入口：sounds/<kind>.{wav,mp3,ogg}，缺文件静默
// 纯头文件（无 Q_OBJECT）：任何页面直接 SoundPlayer::play("match") 即可，
// 不必为了放一声提示音去连 MainWindow 的信号，避免页面间横向依赖。
class SoundPlayer
{
public:
    static void play(const QString &kind, int volume = -1)
    {
        const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/sounds");
        QString path;
        for (const QString &ext : { QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg") }) {
            const QString candidate = dir + QStringLiteral("/%1.%2").arg(kind, ext);
            if (QFile::exists(candidate)) {
                path = candidate;
                break;
            }
        }
        if (path.isEmpty())
            return;

        // 每次播放起一个独立 player：上一声还没播完时不影响下一声，播完自删
        auto *player = new QMediaPlayer(qApp);
        player->setMedia(QUrl::fromLocalFile(path));
        player->setVolume(volume >= 0 ? volume : AppSettings::soundVolume());
        QObject::connect(player, &QMediaPlayer::mediaStatusChanged, player,
                         [player](QMediaPlayer::MediaStatus s) {
                             if (s == QMediaPlayer::EndOfMedia || s == QMediaPlayer::InvalidMedia) {
                                 player->stop();
                                 player->deleteLater();
                             }
                         });
        player->play();
    }
};
