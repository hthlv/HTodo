#pragma once

#include <QColor>
#include <QWidget>

class PomodoroDialWidget : public QWidget {
    Q_OBJECT

public:
    struct ColorPalette {
        QColor track = QColor("#eaeaea");
        QColor progress = QColor("#07c160");
        QColor innerFill = QColor("#ffffff");
        QColor timeText = QColor("#07c160");
    };

    explicit PomodoroDialWidget(QWidget *parent = nullptr);

    void setProgress(double value);
    void setTimeText(const QString &text);
    void setMetaText(const QString &text);
    void setColorPalette(const ColorPalette &palette);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;

private:
    double m_progress = 0.0;
    QString m_timeText;
    QString m_metaText;
    ColorPalette m_palette;
};
