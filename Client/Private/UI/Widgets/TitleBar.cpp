#include "UI/Widgets/TitleBar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>

TitleBar::TitleBar(QWidget *window, const QString &title, QWidget *parent)
    : QWidget(parent), m_window(window)
{
    setObjectName(QStringLiteral("TitleBar"));
    setFixedHeight(54);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *logo = new QLabel(this);
    logo->setObjectName(QStringLiteral("TitleBarLogo"));
    logo->setFixedSize(26, 26);
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/logo.png"))
                        .scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_title = new QLabel(title, this);
    m_title->setObjectName(QStringLiteral("TitleBarTitle"));

    m_section = new QLabel(QStringLiteral("局域网即时通讯"), this);
    m_section->setObjectName(QStringLiteral("TitleBarSubtitle"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 8, 0);
    layout->setSpacing(8);
    layout->addWidget(logo);
    layout->addSpacing(2);
    layout->addWidget(m_title);
    layout->addWidget(m_section);
    layout->addStretch();

    m_minButton = new QPushButton(QStringLiteral("—"), this);
    m_minButton->setObjectName(QStringLiteral("TitleBarButton"));
    m_minButton->setFixedSize(40, 30);
    m_minButton->setCursor(Qt::PointingHandCursor);
    m_minButton->setToolTip(QStringLiteral("最小化"));

    m_maxButton = new QPushButton(QStringLiteral("□"), this);
    m_maxButton->setObjectName(QStringLiteral("TitleBarButton"));
    m_maxButton->setFixedSize(40, 30);
    m_maxButton->setCursor(Qt::PointingHandCursor);
    m_maxButton->setToolTip(QStringLiteral("最大化 / 还原"));

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setObjectName(QStringLiteral("TitleBarClose"));
    m_closeButton->setFixedSize(40, 30);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setToolTip(QStringLiteral("关闭"));

    layout->addWidget(m_minButton);
    layout->addWidget(m_maxButton);
    layout->addWidget(m_closeButton);

    connect(m_minButton, &QPushButton::clicked, m_window, &QWidget::showMinimized);
    connect(m_maxButton, &QPushButton::clicked, this, &TitleBar::toggleMaximize);
    connect(m_closeButton, &QPushButton::clicked, m_window, &QWidget::close);

    m_window->installEventFilter(this);
    updateMaximizeGlyph();
}

void TitleBar::toggleMaximize()
{
    if (!m_window)
        return;
    if (m_window->isMaximized())
        m_window->showNormal();
    else
        m_window->showMaximized();
    updateMaximizeGlyph();
}

void TitleBar::updateMaximizeGlyph()
{
    if (!m_window || !m_maxButton)
        return;
    m_maxButton->setText(m_window->isMaximized() ? QStringLiteral("■")
                                                  : QStringLiteral("□"));
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::WindowStateChange)
        updateMaximizeGlyph();
    return QWidget::eventFilter(watched, event);
}

void TitleBar::setSection(const QString &section)
{
    if (!m_section)
        return;
    if (section.isEmpty())
        m_section->setText(QStringLiteral("局域网即时通讯"));
    else
        m_section->setText(QStringLiteral("/ %1").arg(section));
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragging = true;
    m_dragOffset = event->globalPos() - m_window->frameGeometry().topLeft();
    event->accept();
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton) && m_window && !m_window->isMaximized())
        m_window->move(event->globalPos() - m_dragOffset);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_dragging = false;
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        toggleMaximize();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
