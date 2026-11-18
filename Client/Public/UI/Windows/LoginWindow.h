#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QPoint>
#include <QVector>
#include <QWidget>

class QFrame;
class QGraphicsOpacityEffect;
class QLabel;
class QLineEdit;
class QPushButton;
class LogoMark;
class LoadingSpinner;
class LunaClient;

// 登录窗口：无边框自绘卡片（夜紫渐变 + 星点），
// 进场顺序：连接中转圈 -> 图标居中浮现 -> 图标上浮 -> 登录表单淡入。
// 登录 / 注册是同一个窗口内切换的两张卡片，切换时淡出+滑入。
class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(LunaClient *client, QWidget *parent = nullptr);

signals:
    void loginSucceeded(quint64 userId, const QString &nickname);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onDisconnected();
    void onFrameReceived(const QJsonObject &frame);

private:
    void buildUi();
    void buildLoginCard();
    void buildRegisterCard();
    void startIntro();
    void revealForm();
    void showRegisterPage();
    void showLoginPage();
    void submitRegister();
    void switchCard(QWidget *from, QWidget *to, bool forward);
    void showStatus(const QString &text, int tone = 0);   // 0 普通 / 1 成功 / 2 错误
    void setConnectionState(const QString &text, const QColor &dotColor);
    void sendAuth(const QString &type);

    LunaClient *m_client = nullptr;

    LogoMark *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;

    // 登录卡片
    QFrame *m_card = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_registerButton = nullptr;

    // 注册卡片（内嵌，和登录卡片共用同一个窗口）
    QFrame *m_registerCard = nullptr;
    QLineEdit *m_regUsernameEdit = nullptr;
    QLineEdit *m_regNicknameEdit = nullptr;
    QLineEdit *m_regPasswordEdit = nullptr;
    QLineEdit *m_regConfirmEdit = nullptr;
    QLabel *m_regStatusLabel = nullptr;
    QPushButton *m_regSubmitButton = nullptr;
    QPushButton *m_backButton = nullptr;

    QLabel *m_connDot = nullptr;
    QLabel *m_connText = nullptr;
    QWidget *m_connRow = nullptr;
    QGraphicsOpacityEffect *m_connOpacity = nullptr;
    QLabel *m_footer = nullptr;
    LoadingSpinner *m_spinner = nullptr;
    QLabel *m_spinnerText = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPushButton *m_minButton = nullptr;

    QGraphicsOpacityEffect *m_logoOpacity = nullptr;
    QGraphicsOpacityEffect *m_cardOpacity = nullptr;
    QGraphicsOpacityEffect *m_registerCardOpacity = nullptr;
    QGraphicsOpacityEffect *m_titleOpacity = nullptr;
    QGraphicsOpacityEffect *m_subtitleOpacity = nullptr;
    QGraphicsOpacityEffect *m_spinnerOpacity = nullptr;

    QVector<QPointF> m_stars;
    QVector<qreal> m_starRadius;

    QElapsedTimer m_clock;
    bool m_introStarted = false;
    bool m_introDone = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_switching = false;

    bool m_pendingLogin = false;      // 正在等登录结果
    bool m_pendingRegister = false;   // 正在等注册结果
    bool m_autoLogin = false;         // LUNA_AUTOLOGIN=user:pass
    bool m_loggedIn = false;
    QString m_savedUsername;
    QString m_savedPassword;
};
