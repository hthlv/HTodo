#include "PomodoroFocusCard.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QCloseEvent>
#include <QHideEvent>
#include <QMoveEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "PomodoroDialWidget.h"

namespace {
QIcon createLockIcon(bool locked) {
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(locked ? QColor("#048448") : QColor("#505861"));
    pen.setWidthF(1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    QRectF bodyRect(5.0, 9.0, 10.0, 8.0);
    painter.drawRoundedRect(bodyRect, 2.0, 2.0);

    if (locked) {
        painter.drawArc(QRectF(6.0, 3.0, 8.0, 9.0), 30 * 16, 120 * 16);
        painter.drawPoint(QPointF(10.0, 12.7));
        painter.drawLine(QPointF(10.0, 12.8), QPointF(10.0, 14.9));
    } else {
        painter.drawArc(QRectF(6.0, 3.0, 8.0, 9.0), 30 * 16, 70 * 16);
        painter.drawLine(QPointF(13.3, 6.1), QPointF(15.7, 4.2));
        painter.drawPoint(QPointF(10.0, 12.7));
        painter.drawLine(QPointF(10.0, 12.8), QPointF(10.0, 14.9));
    }

    return QIcon(pixmap);
}
}

PomodoroFocusCard::PomodoroFocusCard(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setWindowTitle("HTodo 沉浸番茄钟");
    setToolTip("拖动移动，双击或按 Esc 返回 HTodo");
    resize(276, 312);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(12, 12, 12, 12);
    outerLayout->setSpacing(0);

    auto *card = new QFrame(this);
    card->setObjectName("focusCardSurface");
    auto *shadowEffect = new QGraphicsDropShadowEffect(card);
    shadowEffect->setBlurRadius(32);
    shadowEffect->setOffset(0, 14);
    shadowEffect->setColor(QColor(28, 40, 35, 48));
    card->setGraphicsEffect(shadowEffect);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 16);
    cardLayout->setSpacing(10);
    cardLayout->setAlignment(Qt::AlignHCenter);

    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(0);

    auto *leadingSpacer = new QWidget(card);
    leadingSpacer->setFixedSize(30, 30);
    headerRow->addWidget(leadingSpacer);
    headerRow->addStretch(1);

    m_lockButton = new QPushButton(card);
    m_lockButton->setObjectName("focusCardLockButton");
    m_lockButton->setCursor(Qt::PointingHandCursor);
    m_lockButton->setFixedSize(30, 30);
    m_lockButton->setText(QString());
    connect(m_lockButton, &QPushButton::clicked, this, [this]() {
        setLocked(!m_locked);
    });
    headerRow->addWidget(m_lockButton, 0, Qt::AlignTop);

    m_unlockButton = new QPushButton(nullptr);
    m_unlockButton->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_unlockButton->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_unlockButton->setAttribute(Qt::WA_TranslucentBackground, true);
    m_unlockButton->setAttribute(Qt::WA_NoSystemBackground, true);
    m_unlockButton->setObjectName("focusCardUnlockButton");
    m_unlockButton->setCursor(Qt::PointingHandCursor);
    m_unlockButton->setFixedSize(30, 30);
    m_unlockButton->setText(QString());
    m_unlockButton->hide();
    connect(m_unlockButton, &QPushButton::clicked, this, [this]() {
        setLocked(false);
    });

    m_phaseLabel = new QLabel("专注阶段", card);
    m_phaseLabel->setObjectName("focusCardPhase");
    m_phaseLabel->setAlignment(Qt::AlignCenter);

    m_dial = new PomodoroDialWidget(card);
    m_dial->setFixedSize(202, 202);
    m_dial->setColorPalette({
        QColor(255, 255, 255, 118),
        QColor("#2dd487"),
        QColor(255, 255, 255, 150),
        QColor("#12b76a"),
    });

    cardLayout->addLayout(headerRow);
    cardLayout->addWidget(m_phaseLabel, 0, Qt::AlignHCenter);
    cardLayout->addWidget(m_dial, 0, Qt::AlignHCenter);
    outerLayout->addWidget(card);

    setStyleSheet(R"(
        QWidget {
            background: transparent;
        }
        QFrame#focusCardSurface {
            background: qlineargradient(
                x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(249, 252, 250, 0.84),
                stop:1 rgba(239, 244, 241, 0.76)
            );
            border: 1px solid rgba(255, 255, 255, 0.82);
            border-radius: 22px;
        }
        QLabel#focusCardPhase {
            background: rgba(232, 245, 237, 0.94);
            color: #0aab59;
            border-radius: 999px;
            padding: 6px 14px;
            font-size: 13px;
            font-weight: 700;
        }
        QPushButton#focusCardLockButton {
            background: rgba(255, 255, 255, 0.52);
            color: #687076;
            border: 1px solid rgba(255, 255, 255, 0.78);
            border-radius: 9px;
            padding: 0;
        }
        QPushButton#focusCardLockButton:hover,
        QPushButton#focusCardUnlockButton:hover {
            background: rgba(255, 255, 255, 0.72);
        }
        QPushButton#focusCardUnlockButton {
            background: rgba(255, 255, 255, 1.0);
            color: #048448;
            border: 1px solid rgba(214, 226, 219, 0.98);
            border-radius: 9px;
            padding: 0;
        }
    )");

    m_unlockButton->setStyleSheet(R"(
        QPushButton {
            background: transparent;
        }
        QPushButton#focusCardUnlockButton {
            background: rgba(255, 255, 255, 1.0);
            color: #048448;
            border: 1px solid rgba(214, 226, 219, 0.98);
            border-radius: 9px;
            padding: 0;
        }
        QPushButton#focusCardUnlockButton:hover {
            background: rgba(245, 250, 247, 1.0);
            border: 1px solid rgba(186, 213, 199, 1.0);
        }
    )");

    updateLockButton();
}

PomodoroFocusCard::~PomodoroFocusCard() {
    if (m_unlockButton != nullptr) {
        m_unlockButton->close();
        delete m_unlockButton;
        m_unlockButton = nullptr;
    }
}

void PomodoroFocusCard::setPhaseText(const QString &text) {
    if (m_phaseLabel != nullptr) {
        m_phaseLabel->setText(text);
    }
}

void PomodoroFocusCard::setTimeText(const QString &text) {
    if (m_dial != nullptr) {
        m_dial->setTimeText(text);
    }
}

void PomodoroFocusCard::setProgress(double value) {
    if (m_dial != nullptr) {
        m_dial->setProgress(value);
    }
}

void PomodoroFocusCard::setLocked(bool locked) {
    if (m_locked == locked) {
        return;
    }

    const bool wasVisible = isVisible();
    const QPoint currentPos = pos();

    m_locked = locked;
    m_dragging = false;
    m_movedWhileDragging = false;
    setWindowFlag(Qt::WindowTransparentForInput, m_locked);
    if (wasVisible) {
        show();
        move(currentPos);
        raise();
    }
    updateLockButton();
    syncUnlockButtonGeometry();

    if (m_unlockButton != nullptr) {
        if (m_locked && wasVisible) {
            m_unlockButton->show();
            m_unlockButton->raise();
        } else {
            m_unlockButton->hide();
        }
    }
}

void PomodoroFocusCard::updateLockButton() {
    if (m_lockButton == nullptr) {
        return;
    }

    const QIcon lockIcon = createLockIcon(m_locked);
    m_lockButton->setVisible(!m_locked);
    m_lockButton->setIcon(lockIcon);
    m_lockButton->setIconSize(QSize(20, 20));
    m_lockButton->setAccessibleName(m_locked ? "解锁沉浸卡片" : "锁定沉浸卡片");
    m_lockButton->setToolTip(m_locked ? "已锁定，点击后可拖动" : "点击后固定位置");

    if (m_unlockButton != nullptr) {
        m_unlockButton->setIcon(lockIcon);
        m_unlockButton->setIconSize(QSize(20, 20));
        m_unlockButton->setAccessibleName(m_locked ? "解锁沉浸卡片" : "锁定沉浸卡片");
        m_unlockButton->setToolTip(m_locked ? "已锁定，点击后可拖动" : "点击后固定位置");
    }
}

void PomodoroFocusCard::syncUnlockButtonGeometry() {
    if (m_unlockButton == nullptr || m_lockButton == nullptr) {
        return;
    }

    const QPoint globalTopLeft = m_lockButton->mapToGlobal(QPoint(0, 0));
    m_unlockButton->move(globalTopLeft);
    m_unlockButton->raise();
}

void PomodoroFocusCard::closeEvent(QCloseEvent *event) {
    emit exitRequested();
    event->ignore();
}

void PomodoroFocusCard::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    syncUnlockButtonGeometry();
}

void PomodoroFocusCard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    syncUnlockButtonGeometry();
}

void PomodoroFocusCard::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    syncUnlockButtonGeometry();
    if (m_unlockButton != nullptr && m_locked) {
        m_unlockButton->show();
        m_unlockButton->raise();
    }
}

void PomodoroFocusCard::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (m_unlockButton != nullptr) {
        m_unlockButton->hide();
    }
}

void PomodoroFocusCard::mousePressEvent(QMouseEvent *event) {
    if (!m_locked && event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_movedWhileDragging = false;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PomodoroFocusCard::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint target = event->globalPosition().toPoint() - m_dragOffset;
        if (target != pos()) {
            m_movedWhileDragging = true;
            move(target);
            emit positionChanged(target);
        }
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void PomodoroFocusCard::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        if (m_movedWhileDragging) {
            emit positionChanged(pos());
        }
    }

    QWidget::mouseReleaseEvent(event);
}

void PomodoroFocusCard::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit exitRequested();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void PomodoroFocusCard::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        emit exitRequested();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}
