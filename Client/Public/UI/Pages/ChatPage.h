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
class QToolButton;
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
                       bool isSelf, const QString &timeText, const QString &dateKey = QString(),
                       qint64 msgId = 0);
    void markRecalled(qint64 msgId);
    void setAvatar(quint64 userId, const QByteArray &imageBytes);
    void clearAvatarCache();
    void setStatusText(const QString &text);
    void setGroupAvatar(const QByteArray &imageBytes);   // 标题栏群头像
    void setMemberCount(int count);                    // 标题栏"N 人"
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
    void groupInfoRequested();          // 点群聊标题栏的"群资料"图标
    void privateSettingsRequested(quint64 peerId);   // 私聊标题栏齿轮：好友设置
    void recallRequested(qint64 msgId); // 右键自己两分钟内的消息撤回
    void groupActivityRequested();      // 点群聊标题栏的"群活动"图标
    void groupInviteRequested();        // 点群聊标题栏的"邀请成员"图标
    void groupFilesRequested();         // 点群聊标题栏的"共享文件夹"图标

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
    QString m_lastDateKey;
    QHash<qint64, QLabel *> m_bubbles;

    QLineEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_fileButton = nullptr;
    QPushButton *m_voiceButton = nullptr;
    QToolButton *m_groupInfoButton = nullptr;
    QToolButton *m_groupActivityButton = nullptr;
    QToolButton *m_groupInviteButton = nullptr;
    QToolButton *m_groupFilesButton = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLabel *m_groupAvatarLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QHash<quint64, QList<QLabel *>> m_avatarLabels;
    QHash<quint64, QByteArray> m_avatars;
    QHash<QObject *, quint64> m_avatarOwners;   // 头像控件 -> 用户，用于右键菜单
    qint64 m_groupId = 0;
    quint64 m_peerId = 0;
};