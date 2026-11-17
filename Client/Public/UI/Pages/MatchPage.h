#pragma once

#include <QJsonObject>
#include <QWidget>

class LunaClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QPropertyAnimation;
class QGraphicsOpacityEffect;

// 随机匹配页面（内嵌）：匹配动画 + 匹配成功后的临时会话
class MatchPage : public QWidget
{
    Q_OBJECT

public:
    MatchPage(LunaClient *client, quint64 userId, const QString &nickname,
              QWidget *parent = nullptr);

private slots:
    void onStart();
    void onCancel();
    void onEnd();
    void onSend();
    void onFrameReceived(const QJsonObject &frame);

private:
    void resetSession();
    void appendLine(const QString &text, bool self);

    LunaClient *m_client = nullptr;
    quint64 m_userId = 0;
    QString m_nickname;

    QString m_sessionId;
    quint64 m_peerId = 0;
    QString m_peerName;

    QComboBox *m_modeBox = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_moonLabel = nullptr;
    QListWidget *m_list = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_endButton = nullptr;
    QPushButton *m_sendButton = nullptr;

    QGraphicsOpacityEffect *m_moonEffect = nullptr;
    QPropertyAnimation *m_moonAnimation = nullptr;
};