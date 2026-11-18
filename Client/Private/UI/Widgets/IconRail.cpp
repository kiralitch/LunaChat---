#include "UI/Widgets/IconRail.h"

#include <QIcon>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

IconRail::IconRail(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("IconRail"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(60);

    // 滑动的选中背景：先创建，后创建的按钮会盖在它上面
    m_highlight = new QWidget(this);
    m_highlight->setObjectName(QStringLiteral("RailHighlight"));
    m_highlight->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_highlight->setFixedSize(44, 44);
    m_highlight->move(8, 10);
    m_highlight->hide();

    m_anim = new QPropertyAnimation(m_highlight, "geometry", this);
    m_anim->setDuration(170);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 10, 6, 10);
    m_layout->setSpacing(8);
    m_layout->addStretch();
}

void IconRail::addItem(const QString &id, const QString &glyph, const QString &tooltip)
{
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("RailButton"));
    button->setText(glyph);
    button->setToolTip(tooltip);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setFixedSize(44, 44);
    button->setCursor(Qt::PointingHandCursor);

    m_layout->insertWidget(m_layout->count() - 1, button, 0, Qt::AlignHCenter);
    m_buttons.insert(id, button);

    connect(button, &QToolButton::clicked, this, [this, id]() {
        if (m_current == id)
            return;
        m_current = id;
        moveHighlight(m_buttons.value(id), true);
        emit currentChanged(id);
    });
}

void IconRail::addIconItem(const QString &id, const QString &iconPath, const QString &tooltip)
{
    addItem(id, QString(), tooltip);
    QToolButton *button = m_buttons.value(id);
    if (!button)
        return;
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(26, 26));
}

void IconRail::setCurrent(const QString &id)
{
    if (!m_buttons.contains(id))
        return;

    m_current = id;
    QToolButton *button = m_buttons.value(id);
    button->setChecked(true);
    // 布局还没算好时 geometry 是默认值，延后一帧再定位
    QTimer::singleShot(0, this, [this, button]() { moveHighlight(button, false); });
}

void IconRail::moveHighlight(QWidget *button, bool animate)
{
    if (!button || !m_highlight)
        return;

    const QRect target = button->geometry();
    if (!animate || !m_highlight->isVisible()) {
        m_anim->stop();
        m_highlight->setGeometry(target);
        m_highlight->show();
        m_highlight->lower();
        return;
    }

    m_anim->stop();
    m_anim->setStartValue(m_highlight->geometry());
    m_anim->setEndValue(target);
    m_anim->start();
    m_highlight->lower();
}

void IconRail::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    moveHighlight(m_buttons.value(m_current), false);
}

void IconRail::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() { moveHighlight(m_buttons.value(m_current), false); });
}