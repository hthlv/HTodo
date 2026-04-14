#pragma once

#include <QObject>
#include <QTimer>

class PomodoroTimer : public QObject {
    Q_OBJECT

public:
    enum class Phase {
        Focus,
        Break
    };

    explicit PomodoroTimer(QObject *parent = nullptr);

    void setDurations(int focusMinutes, int breakMinutes);
    void start();
    void pause();
    void reset();

    bool isRunning() const;
    int remainingSeconds() const;
    Phase phase() const;

signals:
    void tick(int remainingSeconds, Phase phase);
    void phaseCompleted(Phase completedPhase);
    void stateChanged(bool running, Phase phase);

private slots:
    void onTick();

private:
    QTimer m_timer;
    int m_focusMinutes = 25;
    int m_breakMinutes = 5;
    int m_remainingSeconds = 25 * 60;
    Phase m_phase = Phase::Focus;
    bool m_running = false;

    int durationForPhase(Phase phase) const;
    void switchPhase();
    void emitTick();
};
