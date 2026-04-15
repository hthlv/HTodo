#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class TagAnalysisChartWidget : public QWidget {
    Q_OBJECT

public:
    struct BarItem {
        QString label;
        double value = 0.0;
        QString displayValue;
    };

    explicit TagAnalysisChartWidget(QWidget *parent = nullptr);

    void setChartData(const QString &title, const QString &emptyText, const QList<BarItem> &items);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;

private:
    QString m_title;
    QString m_emptyText;
    QList<BarItem> m_items;
};
