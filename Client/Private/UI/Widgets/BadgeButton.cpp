#include "UI/Widgets/BadgeButton.h"

#include <QFont>
#include <QPainter>

BadgeButton::BadgeButton(QWidget *parent)
    : QToolButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setIconSize(QSize(24, 24));
}

void BadgeButton::setCount(int count)
{
    m_count = qMax(0, count);
    setToolTip(m_count > 0 ? QStringLiteral("%1 条待处理申请").arg(m_count)
                           : QStringLiteral("消息与申请"));
    update();
}

void BadgeButton::paintEvent(QPaintEvent *event)
{
    QToolButton::paintEvent(event);
    if (m_count <= 0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool many = m_count > 99;
    const QString text = many ? QStringLiteral("99+") : QString::number(m_count);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(9);
    painter.setFont(font);
    const int textWidth = painter.fontMetrics().horizontalAdvance(text);
    const int diameter = qMax(16, textWidth + 8);
    const QRect badgeRect(width() - diameter - 2, 2, diameter, 16);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xFF, 0x5C, 0x7A));
    painter.drawRoundedRect(badgeRect, 8, 8);

    painter.setPen(Qt::white);
    painter.drawText(badgeRect, Qt::AlignCenter, text);
}
