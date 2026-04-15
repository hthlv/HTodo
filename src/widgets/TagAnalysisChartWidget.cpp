#include "TagAnalysisChartWidget.h"

#include <QPainter>

TagAnalysisChartWidget::TagAnalysisChartWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(220);
}

void TagAnalysisChartWidget::setChartData(const QString &title, const QString &emptyText, const QList<BarItem> &items) {
    m_title = title;
    m_emptyText = emptyText;
    m_items = items;
    updateGeometry();
    update();
}

void TagAnalysisChartWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor("#ffffff"));

    QFont titleFont = font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#191919"));
    painter.drawText(QRect(20, 18, width() - 40, 24), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    if (m_items.isEmpty()) {
        QFont emptyFont = font();
        emptyFont.setPointSize(13);
        painter.setFont(emptyFont);
        painter.setPen(QColor("#8c8c8c"));
        painter.drawText(QRect(20, 54, width() - 40, height() - 74), Qt::AlignCenter, m_emptyText);
        return;
    }

    const int top = 58;
    const int left = 20;
    const int right = width() - 20;
    const int bottom = height() - 20;
    const int rowHeight = 28;
    const int labelWidth = 96;
    const int valueWidth = 84;
    const int barLeft = left + labelWidth + 12;
    const int barRight = right - valueWidth - 10;
    const int barWidth = qMax(40, barRight - barLeft);

    double maxValue = 0.0;
    for (const BarItem &item : m_items) {
        if (item.value > maxValue) {
            maxValue = item.value;
        }
    }
    if (maxValue <= 0.0) {
        maxValue = 1.0;
    }

    QFont labelFont = font();
    labelFont.setPointSize(12);
    painter.setFont(labelFont);

    for (int i = 0; i < m_items.size(); ++i) {
        const BarItem &item = m_items.at(i);
        const int y = top + i * rowHeight;
        const QRect labelRect(left, y, labelWidth, 20);
        const QRect barTrackRect(barLeft, y + 2, barWidth, 16);
        const int fillWidth = static_cast<int>(barWidth * (item.value / maxValue));
        const QRect barFillRect(barLeft, y + 2, qMax(6, fillWidth), 16);
        const QRect valueRect(barRight + 8, y, valueWidth - 8, 20);

        painter.setPen(QColor("#4f5b66"));
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, item.label);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#eef1f4"));
        painter.drawRoundedRect(barTrackRect, 8, 8);
        painter.setBrush(QColor("#07c160"));
        painter.drawRoundedRect(barFillRect, 8, 8);

        painter.setPen(QColor("#191919"));
        painter.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter, item.displayValue);
    }

    painter.setPen(QColor("#eaeaea"));
    painter.drawLine(left, bottom, right, bottom);
}

QSize TagAnalysisChartWidget::sizeHint() const {
    const int rows = qMax(1, m_items.size());
    return QSize(400, 78 + rows * 28 + 20);
}
