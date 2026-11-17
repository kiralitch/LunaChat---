#include "UI/Widgets/AvatarCropper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

AvatarCropper::AvatarCropper(const QImage &image, const QSize &target, QWidget *parent)
    : QDialog(parent), m_image(image), m_target(target.isValid() ? target : QSize(256, 256))
{
    // 取景框按目标比例显示（横幅是 10:3，头像 1:1）
    m_viewW = 320;
    m_viewH = qBound(80, m_viewW * m_target.height() / qMax(1, m_target.width()), 320);

    setWindowTitle(QStringLiteral("调整图片"));

    m_view = new QLabel(this);
    m_view->setFixedSize(m_viewW, m_viewH);
    m_view->setStyleSheet(QStringLiteral("background:#101114;"));
    m_view->setAlignment(Qt::AlignCenter);
    m_view->installEventFilter(this);

    m_zoom = new QSlider(Qt::Horizontal, this);
    m_zoom->setRange(100, 400);
    m_zoom->setValue(100);

    auto *hint = new QLabel(QStringLiteral("拖动图片调整位置，滑杆缩放（输出 %1×%2）")
                                .arg(m_target.width()).arg(m_target.height()), this);
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

    // 初始 100% = 整张图完整可见（contain）
    m_fitScale = qMin(1.0, qMin(static_cast<double>(m_viewW) / qMax(1, m_image.width()),
                               static_cast<double>(m_viewH) / qMax(1, m_image.height())));
    setScale(m_fitScale);
}

QImage AvatarCropper::result() const
{
    const QRectF rect(-m_offset.x() / m_scale, -m_offset.y() / m_scale,
                      m_viewW / m_scale, m_viewH / m_scale);
    const QRect clipped = rect.toRect().intersected(m_image.rect());
    if (clipped.isEmpty())
        return QImage();
    return m_image.copy(clipped).scaled(m_target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

bool AvatarCropper::eventFilter(QObject *watched, QEvent *event)
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

void AvatarCropper::setScale(double scale)
{
    const QPointF center = QPointF(m_viewW, m_viewH) / 2.0;
    const QPointF imagePoint((center.x() - m_offset.x()) / m_scale,
                             (center.y() - m_offset.y()) / m_scale);

    m_scale = qMax(0.05, scale);
    m_offset = QPointF(center.x() - imagePoint.x() * m_scale,
                       center.y() - imagePoint.y() * m_scale);
    clampOffset();
    render();
}

void AvatarCropper::clampOffset()
{
    const double scaledW = m_image.width() * m_scale;
    const double scaledH = m_image.height() * m_scale;

    if (scaledW <= m_viewW)
        m_offset.setX((m_viewW - scaledW) / 2.0);
    else
        m_offset.setX(qBound(m_viewW - scaledW, m_offset.x(), 0.0));

    if (scaledH <= m_viewH)
        m_offset.setY((m_viewH - scaledH) / 2.0);
    else
        m_offset.setY(qBound(m_viewH - scaledH, m_offset.y(), 0.0));
}

void AvatarCropper::render()
{
    QImage canvas(m_viewW, m_viewH, QImage::Format_ARGB32);
    canvas.fill(QColor(QStringLiteral("#101114")));

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF(m_offset, QSizeF(m_image.width() * m_scale,
                                              m_image.height() * m_scale)),
                      m_image, QRectF(m_image.rect()));
    painter.end();

    m_view->setPixmap(QPixmap::fromImage(canvas));
}