#include "PomodoroDialWidget.h"

#include <QPainter>

PomodoroDialWidget::PomodoroDialWidget(QWidget *parent) : QWidget(parent), m_timeText("25:00"), m_metaText("等待开始") {
    setMinimumSize(180, 180);
}

void PomodoroDialWidget::setProgress(double value) {
    const double bounded = qBound(0.0, value, 100.0);
    if (qFuzzyCompare(m_progress, bounded)) {
        return;
    }

    m_progress = bounded;
    update();
}

void PomodoroDialWidget::setTimeText(const QString &text) {
    if (m_timeText == text) {
        return;
    }

    m_timeText = text;
    update();
}

void PomodoroDialWidget::setMetaText(const QString &text) {
    if (m_metaText == text) {
        return;
    }

    m_metaText = text;
    update();
}

void PomodoroDialWidget::setColorPalette(const PomodoroDialWidget::ColorPalette &palette) {
    if (m_palette.track == palette.track
        && m_palette.progress == palette.progress
        && m_palette.innerFill == palette.innerFill
        && m_palette.timeText == palette.timeText) {
        return;
    }

    m_palette = palette;
    update();
}

void PomodoroDialWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect().adjusted(14, 14, -14, -14);
    const QPointF center = bounds.center();
    const qreal diameter = qMin(bounds.width(), bounds.height());
    const QRectF circleRect(center.x() - diameter / 2.0, center.y() - diameter / 2.0, diameter, diameter);

    QPen trackPen(m_palette.track, 14, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(circleRect, 90 * 16, -360 * 16);

    QPen progressPen(m_palette.progress, 14, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(progressPen);
    painter.drawArc(circleRect, 90 * 16, static_cast<int>(-360.0 * 16.0 * (m_progress / 100.0)));

    const QRectF innerRect = circleRect.adjusted(22, 22, -22, -22);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_palette.innerFill);
    painter.drawEllipse(innerRect);

    painter.setPen(m_palette.timeText);
    QFont timeFont = font();
    timeFont.setPointSize(24);
    timeFont.setBold(true);
    painter.setFont(timeFont);
    painter.drawText(innerRect, Qt::AlignCenter, m_timeText);
}

QSize PomodoroDialWidget::sizeHint() const {
    return QSize(180, 180);
}
