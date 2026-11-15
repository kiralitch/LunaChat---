#include "UI/Pages/ProfilePage.h"

#include <QBuffer>
#include <QDateTime>
#include <QFileDialog>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtDebug>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"

namespace {

const int kViewSide = 260;

// 头像裁剪器：滑杆缩放 + 鼠标拖动平移，输出正方形头像（不引任何图像库）
class AvatarCropper : public QDialog
{
public:
    explicit AvatarCropper(const QImage &image, QWidget *parent = nullptr)
        : QDialog(parent), m_image(image)
    {
        setWindowTitle(QStringLiteral("调整头像"));

        m_view = new QLabel(this);
        m_view->setFixedSize(kViewSide, kViewSide);
        m_view->setStyleSheet(QStringLiteral("background:#101114;"));
        m_view->setAlignment(Qt::AlignCenter);
        m_view->installEventFilter(this);

        m_zoom = new QSlider(Qt::Horizontal, this);
        m_zoom->setRange(100, 400);
        m_zoom->setValue(100);

        auto *hint = new QLabel(QStringLiteral("拖动图片调整位置，滑杆缩放"), this);
        hint->setStyleSheet(QStringLiteral("color:#8b8f98;"));

        auto *okButton = new QPushButton(QStringLiteral("确定"), this);
        auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
        auto *buttonRow = new QHBoxLayout;
        buttonRow->addStretch();
        buttonRow->addWidget(okButton);
        buttonRow->addWidget(cancelButton);

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(m_view, 0, Qt::AlignHCenter);
        layout->addWidget(hint);
        layout->addWidget(m_zoom);
        layout->addLayout(buttonRow);

        connect(m_zoom, &QSlider::valueChanged, this, [this](int value) {
            setScale(m_fitScale * value / 100.0);
        });
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

        // 100% = 整张图完整可见（contain），避免大图一进来就只剩中间一块
        m_fitScale = qMin(1.0, static_cast<double>(kViewSide) / qMax(image.width(), image.height()));
    }

    QImage result() const
    {
        // 把取景框（view 正方形）映射回原图坐标，再缩到 256x256
        const QRectF rect(-m_offset.x() / m_scale, -m_offset.y() / m_scale,
                          kViewSide / m_scale, kViewSide / m_scale);
        const QRect clipped = rect.toRect().intersected(m_image.rect());
        if (clipped.isEmpty())
            return QImage();
        return m_image.copy(clipped).scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_view) {
            if (event->type() == QEvent::MouseButtonPress) {
                m_dragging = true;
                m_lastPos = static_cast<QMouseEvent *>(event)->pos();
            } else if (event->type() == QEvent::MouseMove && m_dragging) {
                const QPoint pos = static_cast<QMouseEvent *>(event)->pos();
                m_offset += pos - m_lastPos;
                m_lastPos = pos;
                clampOffset();
                render();
            } else if (event->type() == QEvent::MouseButtonRelease) {
                m_dragging = false;
            }
        }
        return QDialog::eventFilter(watched, event);
    }

private:
    void setScale(double scale)
    {
        const QPointF center = QPointF(kViewSide, kViewSide) / 2.0;
        const QPointF imagePoint((center.x() - m_offset.x()) / m_scale,
                                 (center.y() - m_offset.y()) / m_scale);

        m_scale = qMax(0.05, scale);
        m_offset = QPointF(center.x() - imagePoint.x() * m_scale,
                           center.y() - imagePoint.y() * m_scale);
        clampOffset();
        render();
    }

    void clampOffset()
    {
        const double scaledW = m_image.width() * m_scale;
        const double scaledH = m_image.height() * m_scale;

        // 比取景框小的方向就居中（contain），比取景框大的方向才允许拖动
        if (scaledW <= kViewSide)
            m_offset.setX((kViewSide - scaledW) / 2.0);
        else
            m_offset.setX(qBound(kViewSide - scaledW, m_offset.x(), 0.0));

        if (scaledH <= kViewSide)
            m_offset.setY((kViewSide - scaledH) / 2.0);
        else
            m_offset.setY(qBound(kViewSide - scaledH, m_offset.y(), 0.0));
    }

    void render()
    {
        QImage canvas(kViewSide, kViewSide, QImage::Format_ARGB32);
        canvas.fill(QColor(QStringLiteral("#101114")));

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(QRectF(m_offset, QSizeF(m_image.width() * m_scale,
                                                  m_image.height() * m_scale)),
                          m_image, QRectF(m_image.rect()));
        painter.end();

        m_view->setPixmap(QPixmap::fromImage(canvas));
    }

    QImage m_image;
    QLabel *m_view = nullptr;
    QSlider *m_zoom = nullptr;
    double m_scale = 1.0;
    double m_fitScale = 1.0;
    QPointF m_offset;
    QPoint m_lastPos;
    bool m_dragging = false;
};

}
ProfilePage::ProfilePage(LunaClient *client, quint64 userId, QWidget *parent)
    : QWidget(parent), m_client(client), m_userId(userId)
{
    auto *title = new QLabel(QStringLiteral("个人资料"), this);
    title->setObjectName(QStringLiteral("ChatTitle"));

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setFixedSize(112, 112);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(QStringLiteral(
        "border-radius:56px; background:#5865f2; color:white; font-size:40px; font-weight:700;"));

    m_usernameLabel = new QLabel(this);
    m_usernameLabel->setStyleSheet(QStringLiteral("color:#8b8f98;"));

    m_nicknameEdit = new QLineEdit(this);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("昵称（1-32 字）"));
    m_nicknameEdit->setFixedWidth(260);

    m_statusLabel = new QLabel(QStringLiteral("修改昵称或头像后点保存"), this);

    auto *pickButton = new QPushButton(QStringLiteral("选择头像"), this);
    auto *saveButton = new QPushButton(QStringLiteral("保存"), this);

    connect(pickButton, &QPushButton::clicked, this, &ProfilePage::onPickAvatar);
    connect(saveButton, &QPushButton::clicked, this, &ProfilePage::onSave);
    connect(m_client, &LunaClient::frameReceived, this, &ProfilePage::onFrameReceived);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 20, 28, 20);
    layout->setSpacing(14);
    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addWidget(m_avatarLabel);
    layout->addWidget(m_usernameLabel);
    layout->addWidget(m_nicknameEdit);
    layout->addWidget(pickButton);
    layout->addWidget(saveButton);
    layout->addWidget(m_statusLabel);
    layout->addStretch();
}

void ProfilePage::refresh()
{
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kProfileGet);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), QJsonObject());
    m_client->sendFrame(frame);
}

void ProfilePage::applyAvatar(const QByteArray &bytes)
{
    QImage image;
    if (!image.loadFromData(bytes)) {
        m_statusLabel->setText(QStringLiteral("头像解码失败"));
        return;
    }
    // 圆形头像：短边裁正方形后缩放到 112，再用椭圆裁剪（QSS 的 border-radius 不裁 pixmap）
    const int edge = qMin(image.width(), image.height());
    image = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                    .scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap rounded(112, 112);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addEllipse(0, 0, 112, 112);
    painter.setClipPath(path);
    painter.drawImage(0, 0, image);
    painter.end();

    m_avatarLabel->setPixmap(rounded);
    m_avatarLabel->setStyleSheet(QStringLiteral("background:transparent;"));
}

void ProfilePage::onPickAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), QString(), QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;

    QImage image(path);
    if (image.isNull()) {
        m_statusLabel->setText(QStringLiteral("图片无法读取"));
        return;
    }

    // 先让用户自己缩放/拖动取景，再输出 256x256
    AvatarCropper cropper(image, this);
    if (cropper.exec() != QDialog::Accepted)
        return;
    image = cropper.result();
    if (image.isNull()) {
        m_statusLabel->setText(QStringLiteral("裁剪失败"));
        return;
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPG", 85);

    m_pendingAvatar = bytes;
    applyAvatar(bytes);
    m_statusLabel->setText(QStringLiteral("已选择头像，点保存生效"));
}

void ProfilePage::onSave()
{
    const QString nickname = m_nicknameEdit->text().trimmed();
    const bool nicknameChanged = !nickname.isEmpty() && nickname != m_loadedNickname;
    if (!nicknameChanged && m_pendingAvatar.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("没有要保存的修改"));
        return;
    }

    QJsonObject data;
    if (nicknameChanged)
        data.insert(QStringLiteral("nickname"), nickname);
    if (!m_pendingAvatar.isEmpty())
        data.insert(QStringLiteral("avatarBase64"), QString::fromLatin1(m_pendingAvatar.toBase64()));

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kProfileUpdate);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
    m_statusLabel->setText(QStringLiteral("保存中..."));
}

void ProfilePage::onFrameReceived(const QJsonObject &frame)
{
    if (frame.value(QStringLiteral("type")).toString() != MsgType::kProfileResp)
        return;

    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (data.contains(QStringLiteral("ok"))) {
        const bool ok = data.value(QStringLiteral("ok")).toBool();
        m_statusLabel->setText(data.value(QStringLiteral("msg")).toString());
        if (ok) {
            m_pendingAvatar.clear();
            const QString nickname = data.value(QStringLiteral("nickname")).toString();
            if (!nickname.isEmpty())
                emit nicknameChanged(nickname);
            refresh();   // 用服务端返回的资料刷新，避免"本地看着改了、其实没存上"
        }
        return;
    }

    // profile_get 的返回：填充资料
    m_usernameLabel->setText(QStringLiteral("账号：%1    注册于 %2")
                                 .arg(data.value(QStringLiteral("username")).toString(),
                                      data.value(QStringLiteral("createdAt")).toString()));
    m_loadedNickname = data.value(QStringLiteral("nickname")).toString();
    m_nicknameEdit->setText(m_loadedNickname);

    const QString base64 = data.value(QStringLiteral("avatarBase64")).toString();
    if (!base64.isEmpty())
        applyAvatar(QByteArray::fromBase64(base64.toLatin1()));
    else
        m_avatarLabel->setText(data.value(QStringLiteral("nickname")).toString().left(1));
}
