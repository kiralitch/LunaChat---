#include "UI/Windows/LoginWindow.h"

#include <QAction>
#include <QApplication>
#include <QConicalGradient>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QtDebug>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"

// 图标：按控件自身尺寸平滑缩放（QPainter 的 SmoothPixmapTransform）
class LogoMark : public QWidget
{
public:
    explicit LogoMark(QWidget *parent = nullptr)
        : QWidget(parent), m_pixmap(QStringLiteral(":/icons/logo.png"))
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        if (!m_pixmap.isNull())
            p.drawPixmap(rect(), m_pixmap);
    }

private:
    QPixmap m_pixmap;
};

// 连接中的转圈：锥形渐变的圆弧，定时器推进角度
class LoadingSpinner : public QWidget
{
public:
    explicit LoadingSpinner(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(54, 54);
        m_timer.setInterval(16);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 7) % 360;
            update();
        });
        m_timer.start();
    }

    void stopSpin() { m_timer.stop(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(7, 7, -7, -7);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 26), 4));
        p.drawEllipse(r);

        QConicalGradient cone(r.center(), -m_angle);
        cone.setColorAt(0.00, QColor(124, 92, 255, 0));
        cone.setColorAt(0.35, QColor(124, 92, 255, 140));
        cone.setColorAt(0.75, QColor(77, 141, 255, 255));
        cone.setColorAt(1.00, QColor(77, 141, 255, 0));

        QPen pen(QBrush(cone), 4);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawArc(r, (90 - m_angle) * 16, 300 * 16);
    }

private:
    QTimer m_timer;
    int m_angle = 0;
};

LoginWindow::LoginWindow(LunaClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setWindowTitle(QStringLiteral("LunaChat 月之音 - 登录"));
    setObjectName(QStringLiteral("LoginWindow"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFixedSize(440, 600);
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));

    // 无边框窗口默认居中显示
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.center() - QPoint(width() / 2, height() / 2));
    }

    // 星点固定一份，重绘时直接铺，避免每次随机闪烁
    QRandomGenerator rng(20261118u);
    for (int i = 0; i < 46; ++i) {
        m_stars.append(QPointF(24 + rng.bounded(392), 20 + rng.bounded(560)));
        m_starRadius.append(0.6 + rng.bounded(140) / 100.0);
    }

    buildUi();
    m_clock.start();

    // 调试/自动化演示用：LUNA_AUTOLOGIN=username:password 时自动登录，无需点击
    const QString autoLogin = qEnvironmentVariable("LUNA_AUTOLOGIN");
    if (!autoLogin.isEmpty()) {
        const QStringList parts = autoLogin.split(QLatin1Char(':'));
        if (parts.size() == 2) {
            m_usernameEdit->setText(parts.at(0));
            m_passwordEdit->setText(parts.at(1));
            m_autoLogin = true;
        }
    }

    connect(m_loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);

    // 网络层状态直接反映到界面：连接点 + 文案
    connect(m_client, &LunaClient::connected, this, &LoginWindow::onConnected);
    connect(m_client, &LunaClient::disconnected, this, &LoginWindow::onDisconnected);
    connect(m_client, &LunaClient::frameReceived, this, &LoginWindow::onFrameReceived);
}

void LoginWindow::buildUi()
{
    // 局部样式只作用于登录窗口；全局主题里的 QWidget 背景会盖住自绘，
    // 所以这里先把子控件背景统一置空，再逐条覆盖具体控件。
    setStyleSheet(QStringLiteral(R"(
#LoginWindow { background: transparent; }
#LoginWindow QWidget { background: transparent; }
#LoginWindow QLabel { color: #E9E5F5; }
QLabel#LoginTitle { font-size: 26px; font-weight: 700; color: #FFFFFF; }
QLabel#LoginSubtitle { font-size: 12px; color: #9C93C4; }
QLabel#LoginCaption { font-size: 12px; color: #9C93C4; }
QLabel#LoginStatus { font-size: 12px; color: #9C93C4; }
QLabel#LoginConn { font-size: 11px; color: #8E86B0; }
QLabel#LoginFooter { font-size: 11px; color: rgba(255, 255, 255, 0.26); }
QLabel#LoginSpinnerText { font-size: 12px; color: #A79ECB; }
QFrame#LoginCard {
    background-color: rgba(255, 255, 255, 0.055);
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 18px;
}
QLineEdit#LoginInput {
    background-color: rgba(255, 255, 255, 0.07);
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 12px;
    padding: 0 10px;
    color: #F4F1FF;
    font-size: 14px;
    selection-background-color: #6C5CE7;
    selection-color: #FFFFFF;
}
QLineEdit#LoginInput:focus {
    border: 1px solid rgba(139, 123, 255, 0.85);
    background-color: rgba(255, 255, 255, 0.10);
}
QPushButton#LoginPrimary {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                stop:0 #7C5CFF, stop:1 #4D8DFF);
    border: none;
    border-radius: 12px;
    color: #FFFFFF;
    font-size: 15px;
    font-weight: 600;
}
QPushButton#LoginPrimary:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                stop:0 #8E70FF, stop:1 #5F9BFF);
}
QPushButton#LoginPrimary:pressed {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                stop:0 #6A4BE8, stop:1 #3F7BEA);
}
QPushButton#LoginPrimary:disabled {
    background: rgba(255, 255, 255, 0.10);
    color: rgba(255, 255, 255, 0.40);
}
QPushButton#LoginLink {
    background: transparent;
    border: none;
    color: #B9B0E0;
    font-size: 12px;
}
QPushButton#LoginLink:hover { color: #FFFFFF; }
QPushButton#LoginLink:disabled { color: rgba(255, 255, 255, 0.28); }
QPushButton#LoginWindowButton {
    background: transparent;
    border: none;
    border-radius: 18px;
    color: rgba(255, 255, 255, 0.62);
    font-size: 18px;
}
QPushButton#LoginWindowButton:hover {
    background: rgba(255, 255, 255, 0.14);
    color: #FFFFFF;
}
QPushButton#LoginWindowButton:pressed { background: rgba(255, 255, 255, 0.20); }
)"));

    // ---- 无边框窗口按钮（36×36，比上一版更大） ----
    m_minButton = new QPushButton(QStringLiteral("—"), this);
    m_minButton->setObjectName(QStringLiteral("LoginWindowButton"));
    m_minButton->setGeometry(342, 16, 36, 36);
    m_minButton->setCursor(Qt::PointingHandCursor);
    m_minButton->setToolTip(QStringLiteral("最小化"));
    connect(m_minButton, &QPushButton::clicked, this, &QWidget::showMinimized);

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setObjectName(QStringLiteral("LoginWindowButton"));
    m_closeButton->setGeometry(384, 16, 36, 36);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setToolTip(QStringLiteral("关闭"));
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);

    // ---- 进场：转圈 ----
    m_spinner = new LoadingSpinner(this);
    m_spinner->setGeometry(193, 286, 54, 54);
    m_spinnerOpacity = new QGraphicsOpacityEffect(m_spinner);
    m_spinnerOpacity->setOpacity(1.0);
    m_spinner->setGraphicsEffect(m_spinnerOpacity);

    m_spinnerText = new QLabel(QStringLiteral("正在连接服务器…"), this);
    m_spinnerText->setObjectName(QStringLiteral("LoginSpinnerText"));
    m_spinnerText->setAlignment(Qt::AlignCenter);
    m_spinnerText->setGeometry(70, 354, 300, 24);

    // ---- 图标：先藏在中心，连上后浮现再上浮 ----
    m_logo = new LogoMark(this);
    m_logo->setGeometry(164, 244, 112, 112);
    m_logo->hide();
    m_logoOpacity = new QGraphicsOpacityEffect(m_logo);
    m_logoOpacity->setOpacity(0.0);
    m_logo->setGraphicsEffect(m_logoOpacity);

    m_title = new QLabel(QStringLiteral("LunaChat"), this);
    m_title->setObjectName(QStringLiteral("LoginTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setGeometry(0, 108, width(), 40);
    {
        QFont f = m_title->font();
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        m_title->setFont(f);
    }
    m_title->hide();
    m_titleOpacity = new QGraphicsOpacityEffect(m_title);
    m_titleOpacity->setOpacity(0.0);
    m_title->setGraphicsEffect(m_titleOpacity);

    m_subtitle = new QLabel(QStringLiteral("月 之 音 · 在 月 下 遇 见"), this);
    m_subtitle->setObjectName(QStringLiteral("LoginSubtitle"));
    m_subtitle->setAlignment(Qt::AlignCenter);
    m_subtitle->setGeometry(0, 148, width(), 20);
    m_subtitle->hide();
    m_subtitleOpacity = new QGraphicsOpacityEffect(m_subtitle);
    m_subtitleOpacity->setOpacity(0.0);
    m_subtitle->setGraphicsEffect(m_subtitleOpacity);

    buildLoginCard();
    buildRegisterCard();

    // ---- 底部：连接状态 + 版本 ----
    m_connRow = new QWidget(this);
    m_connRow->setGeometry(0, 522, width(), 24);
    m_connRow->hide();
    m_connOpacity = new QGraphicsOpacityEffect(m_connRow);
    m_connOpacity->setOpacity(0.0);
    m_connRow->setGraphicsEffect(m_connOpacity);

    auto *connLayout = new QHBoxLayout(m_connRow);
    connLayout->setContentsMargins(0, 0, 0, 0);
    connLayout->setSpacing(8);
    connLayout->addStretch();

    m_connDot = new QLabel(m_connRow);
    m_connDot->setFixedSize(8, 8);
    m_connDot->setStyleSheet(QStringLiteral("background: #F0A94C; border-radius: 4px;"));
    m_connText = new QLabel(QStringLiteral("正在连接服务器…"), m_connRow);
    m_connText->setObjectName(QStringLiteral("LoginConn"));

    connLayout->addWidget(m_connDot);
    connLayout->addWidget(m_connText);
    connLayout->addStretch();

    m_footer = new QLabel(QStringLiteral("LunaChat v1.0   ·   TCP 8888"), this);
    m_footer->setObjectName(QStringLiteral("LoginFooter"));
    m_footer->setAlignment(Qt::AlignCenter);
    m_footer->setGeometry(0, 552, width(), 20);
}

namespace {

void applyPlaceholder(QLineEdit *edit)
{
    QPalette pal = edit->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(150, 143, 186));
    edit->setPalette(pal);
}

QLineEdit *makeInput(QWidget *parent, const QString &placeholder, const QString &icon,
                     bool password = false, int height = 48)
{
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(QStringLiteral("LoginInput"));
    edit->setPlaceholderText(placeholder);
    edit->setFixedHeight(height);
    if (password)
        edit->setEchoMode(QLineEdit::Password);
    if (!icon.isEmpty())
        edit->addAction(QIcon(icon), QLineEdit::LeadingPosition);
    applyPlaceholder(edit);
    return edit;
}

}   // namespace

void LoginWindow::buildLoginCard()
{
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("LoginCard"));
    m_card->setGeometry(44, 182, 352, 300);
    m_card->hide();
    m_cardOpacity = new QGraphicsOpacityEffect(m_card);
    m_cardOpacity->setOpacity(0.0);
    m_card->setGraphicsEffect(m_cardOpacity);

    auto *layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(10);

    m_usernameEdit = makeInput(m_card, QStringLiteral("用户名"), QStringLiteral(":/icons/user.png"));
    m_usernameEdit->setText(QStringLiteral("luna"));   // 演示方便：预填测试账号

    m_passwordEdit = makeInput(m_card, QStringLiteral("密码"), QStringLiteral(":/icons/lock.png"), true);
    m_passwordEdit->setText(QStringLiteral("123456"));

    QAction *reveal = m_passwordEdit->addAction(QIcon(QStringLiteral(":/icons/eye.png")),
                                                QLineEdit::TrailingPosition);
    reveal->setToolTip(QStringLiteral("显示 / 隐藏密码"));
    connect(reveal, &QAction::triggered, this, [this, reveal]() {
        const bool hidden = m_passwordEdit->echoMode() == QLineEdit::Password;
        m_passwordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        reveal->setIcon(QIcon(hidden ? QStringLiteral(":/icons/eye_off.png")
                                     : QStringLiteral(":/icons/eye.png")));
    });

    m_loginButton = new QPushButton(QStringLiteral("登 录"), m_card);
    m_loginButton->setObjectName(QStringLiteral("LoginPrimary"));
    m_loginButton->setFixedHeight(48);
    m_loginButton->setCursor(Qt::PointingHandCursor);
    m_loginButton->setEnabled(false);
    m_loginButton->setDefault(true);

    m_statusLabel = new QLabel(m_card);
    m_statusLabel->setObjectName(QStringLiteral("LoginStatus"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setFixedHeight(18);

    m_registerButton = new QPushButton(QStringLiteral("还没有账号？  立即注册"), m_card);
    m_registerButton->setObjectName(QStringLiteral("LoginLink"));
    m_registerButton->setFixedHeight(30);
    m_registerButton->setCursor(Qt::PointingHandCursor);
    m_registerButton->setEnabled(false);

    layout->addWidget(m_usernameEdit);
    layout->addWidget(m_passwordEdit);
    layout->addWidget(m_loginButton);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_registerButton);
}

void LoginWindow::buildRegisterCard()
{
    m_registerCard = new QFrame(this);
    m_registerCard->setObjectName(QStringLiteral("LoginCard"));   // 复用登录卡片的样式
    m_registerCard->setGeometry(44, 182, 352, 324);
    m_registerCard->hide();
    m_registerCardOpacity = new QGraphicsOpacityEffect(m_registerCard);
    m_registerCardOpacity->setOpacity(0.0);
    m_registerCard->setGraphicsEffect(m_registerCardOpacity);

    auto *layout = new QVBoxLayout(m_registerCard);
    layout->setContentsMargins(24, 14, 24, 12);
    layout->setSpacing(5);

    auto *caption = new QLabel(QStringLiteral("创建你的 LunaChat 账号"), m_registerCard);
    caption->setObjectName(QStringLiteral("LoginCaption"));
    caption->setFixedHeight(18);

    m_regUsernameEdit = makeInput(m_registerCard, QStringLiteral("用户名（登录用，唯一）"),
                                  QStringLiteral(":/icons/user.png"), false, 40);
    m_regNicknameEdit = makeInput(m_registerCard, QStringLiteral("昵称（别人看到的名字）"),
                                  QStringLiteral(":/icons/user.png"), false, 40);
    m_regPasswordEdit = makeInput(m_registerCard, QStringLiteral("密码"),
                                  QStringLiteral(":/icons/lock.png"), true, 40);
    m_regConfirmEdit = makeInput(m_registerCard, QStringLiteral("确认密码"),
                                 QStringLiteral(":/icons/lock.png"), true, 40);

    m_regSubmitButton = new QPushButton(QStringLiteral("注 册"), m_registerCard);
    m_regSubmitButton->setObjectName(QStringLiteral("LoginPrimary"));
    m_regSubmitButton->setFixedHeight(42);
    m_regSubmitButton->setCursor(Qt::PointingHandCursor);

    m_regStatusLabel = new QLabel(m_registerCard);
    m_regStatusLabel->setObjectName(QStringLiteral("LoginStatus"));
    m_regStatusLabel->setAlignment(Qt::AlignCenter);
    m_regStatusLabel->setFixedHeight(16);

    m_backButton = new QPushButton(QStringLiteral("已有账号？  返回登录"), m_registerCard);
    m_backButton->setObjectName(QStringLiteral("LoginLink"));
    m_backButton->setFixedHeight(24);
    m_backButton->setCursor(Qt::PointingHandCursor);

    layout->addWidget(caption);
    layout->addWidget(m_regUsernameEdit);
    layout->addWidget(m_regNicknameEdit);
    layout->addWidget(m_regPasswordEdit);
    layout->addWidget(m_regConfirmEdit);
    layout->addWidget(m_regSubmitButton);
    layout->addWidget(m_regStatusLabel);
    layout->addWidget(m_backButton);

    connect(m_regSubmitButton, &QPushButton::clicked, this, &LoginWindow::submitRegister);
    connect(m_backButton, &QPushButton::clicked, this, &LoginWindow::showLoginPage);
    connect(m_regConfirmEdit, &QLineEdit::returnPressed, this, &LoginWindow::submitRegister);
}

void LoginWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF outer = QRectF(rect()).adjusted(10, 10, -10, -10);

    // 柔和投影（多层半透明圆角矩形叠加，避免额外依赖）
    p.setPen(Qt::NoPen);
    for (int i = 10; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, 4));
        p.drawRoundedRect(outer.adjusted(-i, -i + 4, i, i + 4), 24 + i, 24 + i);
    }

    QPainterPath path;
    path.addRoundedRect(outer, 24, 24);

    QLinearGradient bg(outer.topLeft(), outer.bottomRight());
    bg.setColorAt(0.00, QColor(36, 27, 78));
    bg.setColorAt(0.55, QColor(25, 18, 51));
    bg.setColorAt(1.00, QColor(13, 10, 28));
    p.fillPath(path, bg);

    p.save();
    p.setClipPath(path);

    // 顶部紫色光晕：衬托图标
    QRadialGradient glow(QPointF(width() / 2.0, 96), 300);
    glow.setColorAt(0.0, QColor(124, 92, 255, 78));
    glow.setColorAt(1.0, QColor(124, 92, 255, 0));
    p.fillRect(rect(), glow);

    // 左下冷色、右下月光的柔光
    QRadialGradient cool(QPointF(40, 560), 320);
    cool.setColorAt(0.0, QColor(45, 110, 255, 40));
    cool.setColorAt(1.0, QColor(45, 110, 255, 0));
    p.fillRect(rect(), cool);

    QRadialGradient moon(QPointF(420, 580), 240);
    moon.setColorAt(0.0, QColor(255, 236, 190, 24));
    moon.setColorAt(1.0, QColor(255, 236, 190, 0));
    p.fillRect(rect(), moon);

    // 星点
    for (int i = 0; i < m_stars.size(); ++i) {
        const qreal r = m_starRadius.at(i);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 36 + int(r * 42)));
        p.drawEllipse(m_stars.at(i), r, r);
    }

    // 顶部高光
    QLinearGradient highlight(outer.topLeft(), QPointF(outer.left(), outer.top() + 150));
    highlight.setColorAt(0.0, QColor(255, 255, 255, 14));
    highlight.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(outer.left(), outer.top(), outer.width(), 150), highlight);

    p.restore();

    // 细边框
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.drawPath(path);
}

void LoginWindow::startIntro()
{
    if (m_introStarted)
        return;
    m_introStarted = true;

    // 1) 转圈淡出
    auto *spinFade = new QPropertyAnimation(m_spinnerOpacity, "opacity", this);
    spinFade->setDuration(220);
    spinFade->setStartValue(1.0);
    spinFade->setEndValue(0.0);
    connect(spinFade, &QPropertyAnimation::finished, this, [this]() {
        m_spinner->stopSpin();
        m_spinner->hide();
        m_spinnerText->hide();

        // 2) 图标在中心浮现（放大 + 淡入）
        m_logo->show();
        m_logo->setGeometry(176, 256, 88, 88);
        auto *fade = new QPropertyAnimation(m_logoOpacity, "opacity", this);
        fade->setDuration(300);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->start(QAbstractAnimation::DeleteWhenStopped);

        auto *grow = new QPropertyAnimation(m_logo, "geometry", this);
        grow->setDuration(340);
        grow->setStartValue(QRect(176, 256, 88, 88));
        grow->setEndValue(QRect(164, 244, 112, 112));
        grow->setEasingCurve(QEasingCurve::OutCubic);
        grow->start(QAbstractAnimation::DeleteWhenStopped);

        // 3) 停留片刻后上浮到页面顶部
        QTimer::singleShot(760, this, [this]() {
            auto *move = new QPropertyAnimation(m_logo, "geometry", this);
            move->setDuration(700);
            move->setStartValue(m_logo->geometry());
            move->setEndValue(QRect(188, 40, 64, 64));
            move->setEasingCurve(QEasingCurve::OutCubic);
            connect(move, &QPropertyAnimation::finished, this, &LoginWindow::revealForm);
            move->start(QAbstractAnimation::DeleteWhenStopped);

            // 标题随图标上浮一起出现
            auto *titleFade = new QPropertyAnimation(m_titleOpacity, "opacity", this);
            titleFade->setDuration(520);
            titleFade->setStartValue(0.0);
            titleFade->setEndValue(1.0);

            auto *subtitleFade = new QPropertyAnimation(m_subtitleOpacity, "opacity", this);
            subtitleFade->setDuration(620);
            subtitleFade->setStartValue(0.0);
            subtitleFade->setEndValue(1.0);

            QTimer::singleShot(180, this, [titleFade, subtitleFade]() {
                titleFade->start(QAbstractAnimation::DeleteWhenStopped);
                subtitleFade->start(QAbstractAnimation::DeleteWhenStopped);
            });
            m_title->show();
            m_subtitle->show();
        });
    });
    spinFade->start(QAbstractAnimation::DeleteWhenStopped);
}

void LoginWindow::revealForm()
{
    if (m_introDone)
        return;
    m_introDone = true;

    const QRect target(44, 182, 352, 300);
    m_card->show();
    m_card->setGeometry(target.translated(0, 16));
    m_cardOpacity->setOpacity(0.0);

    auto *fade = new QPropertyAnimation(m_cardOpacity, "opacity", this);
    fade->setDuration(430);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    auto *slide = new QPropertyAnimation(m_card, "geometry", this);
    slide->setDuration(430);
    slide->setStartValue(target.translated(0, 16));
    slide->setEndValue(target);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    slide->start(QAbstractAnimation::DeleteWhenStopped);

    const bool online = m_client && m_client->state() == LunaClient::Connected;
    m_loginButton->setEnabled(online);
    m_registerButton->setEnabled(online);

    m_connRow->show();
    auto *connFade = new QPropertyAnimation(m_connOpacity, "opacity", this);
    connFade->setDuration(380);
    connFade->setStartValue(0.0);
    connFade->setEndValue(1.0);
    connFade->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(460, this, [this]() {
        if (isVisible() && m_card->isVisible())
            m_usernameEdit->setFocus();
    });
}

void LoginWindow::showRegisterPage()
{
    if (m_switching || m_card->isHidden())
        return;

    m_pendingRegister = false;
    m_regStatusLabel->clear();
    m_regUsernameEdit->clear();
    m_regNicknameEdit->clear();
    m_regPasswordEdit->clear();
    m_regConfirmEdit->clear();
    m_regSubmitButton->setEnabled(m_client && m_client->state() == LunaClient::Connected);

    switchCard(m_card, m_registerCard, true);
}

void LoginWindow::showLoginPage()
{
    if (m_switching || m_registerCard->isHidden())
        return;
    switchCard(m_registerCard, m_card, false);
}

void LoginWindow::switchCard(QWidget *from, QWidget *to, bool forward)
{
    m_switching = true;

    auto *fromEff = qobject_cast<QGraphicsOpacityEffect *>(from->graphicsEffect());
    auto *toEff = qobject_cast<QGraphicsOpacityEffect *>(to->graphicsEffect());
    if (!fromEff || !toEff) {
        m_switching = false;
        return;
    }

    const QRect target = (to == m_card) ? QRect(44, 182, 352, 300)
                                        : QRect(44, 182, 352, 324);
    const int dx = forward ? 18 : -18;

    m_card->setEnabled(false);
    m_registerCard->setEnabled(false);

    auto *out = new QPropertyAnimation(fromEff, "opacity", this);
    out->setDuration(150);
    out->setStartValue(fromEff->opacity());
    out->setEndValue(0.0);
    connect(out, &QPropertyAnimation::finished, this,
            [this, from, to, toEff, target, dx]() {
        from->hide();
        from->setGeometry(target);

        to->setGeometry(target.translated(dx, 0));
        to->show();
        toEff->setOpacity(0.0);

        auto *in = new QPropertyAnimation(toEff, "opacity", this);
        in->setDuration(280);
        in->setStartValue(0.0);
        in->setEndValue(1.0);
        in->start(QAbstractAnimation::DeleteWhenStopped);

        auto *slide = new QPropertyAnimation(to, "geometry", this);
        slide->setDuration(300);
        slide->setStartValue(target.translated(dx, 0));
        slide->setEndValue(target);
        slide->setEasingCurve(QEasingCurve::OutCubic);
        slide->start(QAbstractAnimation::DeleteWhenStopped);

        QTimer::singleShot(340, this, [this]() {
            m_switching = false;
            m_card->setEnabled(true);
            m_registerCard->setEnabled(true);
            if (m_card->isVisible())
                m_usernameEdit->setFocus();
            else
                m_regUsernameEdit->setFocus();
        });
    });
    out->start(QAbstractAnimation::DeleteWhenStopped);
}

void LoginWindow::submitRegister()
{
    if (m_switching || m_pendingRegister)
        return;

    const QString username = m_regUsernameEdit->text().trimmed();
    const QString password = m_regPasswordEdit->text();
    const QString nickname = m_regNicknameEdit->text().trimmed();

    auto fail = [this](const QString &text) {
        m_regStatusLabel->setStyleSheet(QStringLiteral("color: #FF8E8E;"));
        m_regStatusLabel->setText(text);
    };

    if (m_client->state() != LunaClient::Connected) {
        fail(QStringLiteral("尚未连接服务器，请稍候"));
        return;
    }
    if (username.isEmpty() || password.isEmpty()) {
        fail(QStringLiteral("用户名和密码不能为空"));
        return;
    }
    if (password != m_regConfirmEdit->text()) {
        fail(QStringLiteral("两次输入的密码不一致"));
        return;
    }

    QJsonObject data;
    data.insert(QStringLiteral("username"), username);
    data.insert(QStringLiteral("password"), password);
    data.insert(QStringLiteral("nickname"), nickname.isEmpty() ? username : nickname);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kRegister);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);

    m_pendingRegister = true;
    m_regSubmitButton->setEnabled(false);
    m_regStatusLabel->setStyleSheet(QStringLiteral("color: #9C93C4;"));
    m_regStatusLabel->setText(QStringLiteral("注册中…"));
}

void LoginWindow::onConnected()
{
    setConnectionState(QStringLiteral("已连接服务器"), QColor(87, 217, 163));

    if (!m_introStarted) {
        // 本地连接太快，至少让转圈露个面，观感才像"启动动画"
        const qint64 elapsed = m_clock.isValid() ? m_clock.elapsed() : 0;
        const int wait = qMax(0, 700 - int(elapsed));
        QTimer::singleShot(wait, this, [this]() { startIntro(); });
    }

    // 自动化/演示：首次连上就直接登录
    if (m_autoLogin && !m_loggedIn) {
        m_autoLogin = false;
        onLoginClicked();
        return;
    }

    // 曾经登录成功过：重连成功后自动重登，用户不用重新输密码
    if (m_loggedIn) {
        QJsonObject data;
        data.insert(QStringLiteral("username"), m_savedUsername);
        data.insert(QStringLiteral("password"), m_savedPassword);
        data.insert(QStringLiteral("nickname"), QString());

        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kLogin);
        frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        frame.insert(QStringLiteral("data"), data);
        m_client->sendFrame(frame);
        m_pendingLogin = false;
        showStatus(QStringLiteral("连接已恢复，正在自动重登…"));
    }
}

void LoginWindow::onDisconnected()
{
    setConnectionState(QStringLiteral("连接已断开，正在重连…"), QColor(255, 128, 128));
    m_loginButton->setEnabled(false);
    m_registerButton->setEnabled(false);
    m_regSubmitButton->setEnabled(false);

    if (!m_introStarted)
        m_spinnerText->setText(QStringLiteral("连接失败，正在重试…"));
    else if (m_introDone)
        showStatus(QStringLiteral("连接已断开，正在自动重连…"), 2);
}

void LoginWindow::setConnectionState(const QString &text, const QColor &dotColor)
{
    m_connText->setText(text);
    m_connDot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;")
                                 .arg(dotColor.name()));
}

void LoginWindow::showStatus(const QString &text, int tone)
{
    static const char *const colors[] = { "#9C93C4", "#6FE3B4", "#FF8E8E" };
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1;")
                                     .arg(QString::fromLatin1(colors[qBound(0, tone, 2)])));
}

void LoginWindow::sendAuth(const QString &type)
{
    QJsonObject data;
    data.insert(QStringLiteral("username"), m_usernameEdit->text().trimmed());
    data.insert(QStringLiteral("password"), m_passwordEdit->text());
    data.insert(QStringLiteral("nickname"), QString());

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), type);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
}

void LoginWindow::onLoginClicked()
{
    if (m_client->state() != LunaClient::Connected) {
        showStatus(QStringLiteral("尚未连接服务器，请稍候"), 2);
        return;
    }
    m_pendingLogin = true;
    sendAuth(MsgType::kLogin);
    showStatus(QStringLiteral("登录中…"));
}

void LoginWindow::onRegisterClicked()
{
    if (m_client->state() != LunaClient::Connected) {
        showStatus(QStringLiteral("尚未连接服务器，请稍候"), 2);
        return;
    }
    showRegisterPage();
}

void LoginWindow::onFrameReceived(const QJsonObject &frame)
{
    if (frame.value(QStringLiteral("type")).toString() != MsgType::kLoginResp)
        return;

    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const bool ok = data.value(QStringLiteral("ok")).toBool();
    const QString msg = data.value(QStringLiteral("msg")).toString();

    // 注册结果：停留在注册页提示；成功后回到登录页并带回用户名
    if (m_pendingRegister) {
        m_pendingRegister = false;
        m_regSubmitButton->setEnabled(true);

        if (!ok) {
            m_regStatusLabel->setStyleSheet(QStringLiteral("color: #FF8E8E;"));
            m_regStatusLabel->setText(msg);
            return;
        }

        m_usernameEdit->setText(m_regUsernameEdit->text().trimmed());
        m_passwordEdit->clear();
        m_regPasswordEdit->clear();
        m_regConfirmEdit->clear();
        showLoginPage();
        showStatus(QStringLiteral("注册成功，请登录"), 1);
        QTimer::singleShot(380, this, [this]() { m_passwordEdit->setFocus(); });
        return;
    }

    if (!ok) {
        showStatus(msg, 2);
        m_pendingLogin = false;
        return;
    }

    if (m_pendingLogin) {
        const quint64 userId = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());
        showStatus(QStringLiteral("登录成功，正在进入月之音…"), 1);

        // 记住本次凭据（仅内存），供断线重连后自动重登
        m_loggedIn = true;
        m_savedUsername = m_usernameEdit->text().trimmed();
        m_savedPassword = m_passwordEdit->text();

        emit loginSucceeded(userId, data.value(QStringLiteral("nickname")).toString());
    } else if (m_loggedIn) {
        showStatus(QStringLiteral("已自动重登"), 1);
    }
}

void LoginWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void LoginWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
        move(event->globalPos() - m_dragOffset);
}

void LoginWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_dragging = false;
}
