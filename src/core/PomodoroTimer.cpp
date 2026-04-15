#include "PomodoroTimer.h"

#include <QtGlobal>

PomodoroTimer::PomodoroTimer(QObject *parent) : QObject(parent) {
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &PomodoroTimer::onTick);
}

void PomodoroTimer::setDurations(int focusMinutes, int breakMinutes) {
    m_focusMinutes = qBound(1, focusMinutes, 240);
    m_breakMinutes = qBound(1, breakMinutes, 120);

    if (!m_running) {
        m_remainingSeconds = durationForPhase(m_phase);
        emitTick();
    }
}

void PomodoroTimer::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    m_timer.start();
    emit stateChanged(m_running, m_phase);
}

void PomodoroTimer::pause() {
    if (!m_running) {
        return;
    }

    m_running = false;
    m_timer.stop();
    emit stateChanged(m_running, m_phase);
}

void PomodoroTimer::reset() {
    m_running = false;
    m_timer.stop();
    m_phase = Phase::Focus;
    m_remainingSeconds = durationForPhase(m_phase);

    emitTick();
    emit stateChanged(m_running, m_phase);
}

bool PomodoroTimer::isRunning() const {
    return m_running;
}

int PomodoroTimer::remainingSeconds() const {
    return m_remainingSeconds;
}

PomodoroTimer::Phase PomodoroTimer::phase() const {
    return m_phase;
}

void PomodoroTimer::onTick() {
    if (m_remainingSeconds > 0) {
        --m_remainingSeconds;
    }

    if (m_remainingSeconds <= 0) {
        emit phaseCompleted(m_phase);
        switchPhase();
        emit stateChanged(m_running, m_phase);
    }

    emitTick();
}

int PomodoroTimer::durationForPhase(PomodoroTimer::Phase phase) const {
    return (phase == Phase::Focus ? m_focusMinutes : m_breakMinutes) * 60;
}

void PomodoroTimer::switchPhase() {
    m_phase = (m_phase == Phase::Focus) ? Phase::Break : Phase::Focus;
    m_remainingSeconds = durationForPhase(m_phase);
}

void PomodoroTimer::emitTick() {
    emit tick(m_remainingSeconds, m_phase);
}
