#pragma once
#include <QByteArray>
#include <QHash>
#include <QList>

#include <QWidget>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

// 聊天页：群聊与私聊共用，只负责显示和输入；发送意图交给 MainWindow 组帧
class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);

    void openGroup(qint64 groupId, const QString &title);
    void openPrivate(quint64 peerId, const QString &nickname);
    void clearMessages();
    void appendMessage(quint64 userId, const QString &nickname, const QString &content,
                       bool isSelf, const QString &timeText);
    void setAvatar(quint64 userId, const QByteArray &imageBytes);
    void clearAvatarCache();
    void setStatusText(const QString &text);
    void appendVoiceMessage(const QString &nickname, const QString &timeText,
                            const QString &filePath, int durationMs, bool self);
    void setVoiceButtonText(const QString &text);
    void showProgress(qint64 sent, qint64 total);
    void hideProgress();

    qint64 groupId() const { return m_groupId; }
    quint64 peerId() const { return m_peerId; }

signals:
    void sendRequested(const QString &text);
    void fileSendRequested();
    void voiceRecordRequested();
    void avatarContextMenuRequested(quint64 userId, const QPoint &globalPos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSendClicked();

private:
    void addRow(QWidget *row);
    void scrollToBottom();

    QScrollArea *m_scroll = nullptr;
    QWidget *m_msgContainer = nullptr;
    QVBoxLayout *m_msgLayout = nullptr;

    QLineEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_fileButton = nullptr;
    QPushButton *m_voiceButton = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QHash<quint64, QList<QLabel *>> m_avatarLabels;
    QHash<quint64, QByteArray> m_avatars;
    QHash<QObject *, quint64> m_avatarOwners;   // 头像控件 -> 用户，用于右键菜单
    qint64 m_groupId = 0;
    quint64 m_peerId = 0;
};