#pragma once

#include <QWidget>

class QLabel;
class PomodoroDialWidget;
class QPushButton;

class PomodoroFocusCard : public QWidget {
    Q_OBJECT

public:
    explicit PomodoroFocusCard(QWidget *parent = nullptr);
    ~PomodoroFocusCard() override;

    void setPhaseText(const QString &text);
    void setTimeText(const QString &text);
    void setProgress(double value);
    void setLocked(bool locked);

signals:
    void exitRequested();
    void positionChanged(const QPoint &topLeft);

protected:
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void updateLockButton();
    void syncUnlockButtonGeometry();

    QLabel *m_phaseLabel = nullptr;
    PomodoroDialWidget *m_dial = nullptr;
    QPushButton *m_lockButton = nullptr;
    QPushButton *m_unlockButton = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_movedWhileDragging = false;
    bool m_locked = false;
};
