#include "UI/Widgets/AvatarCropper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {
const int kViewSide = 260;
}

AvatarCropper::AvatarCropper(const QImage &image, QWidget *parent)
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

    m_fitScale = qMin(1.0, static_cast<double>(kViewSide) / qMax(image.width(), image.height()));
    setScale(m_fitScale);
}

QImage AvatarCropper::result() const
{
    const QRectF rect(-m_offset.x() / m_scale, -m_offset.y() / m_scale,
                      kViewSide / m_scale, kViewSide / m_scale);
    const QRect clipped = rect.toRect().intersected(m_image.rect());
    if (clipped.isEmpty())
        return QImage();
    return m_image.copy(clipped).scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
    const QPointF center = QPointF(kViewSide, kViewSide) / 2.0;
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

    if (scaledW <= kViewSide)
        m_offset.setX((kViewSide - scaledW) / 2.0);
    else
        m_offset.setX(qBound(kViewSide - scaledW, m_offset.x(), 0.0));

    if (scaledH <= kViewSide)
        m_offset.setY((kViewSide - scaledH) / 2.0);
    else
        m_offset.setY(qBound(kViewSide - scaledH, m_offset.y(), 0.0));
}

void AvatarCropper::render()
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