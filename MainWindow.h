#pragma once

#include <QMainWindow>
#include <QSet>

#include "PomodoroTimer.h"
#include "PomodoroDialWidget.h"
#include "PomodoroFocusCard.h"
#include "TagAnalysisChartWidget.h"
#include "TaskStorage.h"

class QCalendarWidget;
class QCheckBox;
class QDateEdit;
class QDateTimeEdit;
class QBoxLayout;
class QHBoxLayout;
class QCloseEvent;
class QFrame;
class QGridLayout;
class QLayout;
class QLineEdit;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QSpinBox;
class QSystemTrayIcon;
class QTabWidget;
class QTimer;
class QVBoxLayout;
class QWidget;
class QAction;
class QMenu;
class QLocalServer;
class RoundedComboBox;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void attachSingleInstanceServer(QLocalServer *server);
    void activateFromSingleInstanceMessage();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void addTodo();
    void cancelTodoEdit();
    void onTodoItemChanged(QListWidgetItem *item);
    void onTodoSelectionChanged();

    void togglePomodoro();
    void resetPomodoro();
    void onPomodoroTick(int remainingSeconds, PomodoroTimer::Phase phase);
    void onPomodoroPhaseCompleted(PomodoroTimer::Phase completedPhase);

    void refreshAll();
    void refreshTaskTimingUi();

private:
    enum class TodoListMode {
        Today,
        All,
        Archive
    };

    enum class WindowLayoutMode {
        Standard,
        Compact
    };

    TaskStorage m_storage;
    PomodoroTimer m_timer;
    bool m_pomodoroTaskSelectionLocked = false;
    bool m_refreshingTodoList = false;
    QString m_editingTodoId;
    int m_lastOverdueCount = -1;
    QDate m_selectedDate = QDate::currentDate();
    QSet<QDate> m_calendarMarkedDates;
    WindowLayoutMode m_windowLayoutMode = WindowLayoutMode::Standard;
    bool m_quitRequested = false;
    bool m_trayHintShown = false;
    QPoint m_lastFocusCardTopLeft = QPoint(-1, -1);

    QTabWidget *m_tabs = nullptr;
    RoundedComboBox *m_layoutModeSelector = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showWindowAction = nullptr;
    QAction *m_hideWindowAction = nullptr;
    QAction *m_quitAction = nullptr;
    QLocalServer *m_singleInstanceServer = nullptr;

    QLabel *m_todayLabel = nullptr;
    QLabel *m_todoSummaryLabel = nullptr;
    QLabel *m_overdueReminderLabel = nullptr;
    QLabel *m_selectedDateLabel = nullptr;
    QLabel *m_selectedDateMetaLabel = nullptr;
    QLabel *m_compactSelectedDateLabel = nullptr;
    QLabel *m_compactTaskCountLabel = nullptr;
    QLabel *m_compactDoneCountLabel = nullptr;
    QLabel *m_compactFocusCountLabel = nullptr;
    QLabel *m_dayTaskCountLabel = nullptr;
    QLabel *m_dayDoneCountLabel = nullptr;
    QLabel *m_dayFocusCountLabel = nullptr;
    QLabel *m_editorCaptionLabel = nullptr;
    QLineEdit *m_todoInput = nullptr;
    QCalendarWidget *m_dateNavigator = nullptr;
    QPushButton *m_prevDayButton = nullptr;
    QPushButton *m_todayQuickButton = nullptr;
    QPushButton *m_nextDayButton = nullptr;
    QDateEdit *m_taskDateInput = nullptr;
    QCalendarWidget *m_taskDatePopupCalendar = nullptr;
    QCheckBox *m_dailyPlanEnabled = nullptr;
    QDateEdit *m_planEndDateInput = nullptr;
    QCalendarWidget *m_planEndDatePopupCalendar = nullptr;
    RoundedComboBox *m_priorityInput = nullptr;
    QCheckBox *m_dueAtEnabled = nullptr;
    QDateTimeEdit *m_dueAtInput = nullptr;
    QCalendarWidget *m_dueAtPopupCalendar = nullptr;
    QLineEdit *m_tagInput = nullptr;
    QPushButton *m_addTodoButton = nullptr;
    QPushButton *m_cancelEditButton = nullptr;
    QPushButton *m_filterToggleButton = nullptr;
    RoundedComboBox *m_viewModeFilter = nullptr;
    RoundedComboBox *m_priorityFilter = nullptr;
    QLineEdit *m_tagFilterInput = nullptr;
    QPushButton *m_clearFilterButton = nullptr;
    QWidget *m_filterPopup = nullptr;
    QWidget *m_tagPopup = nullptr;
    QScrollArea *m_tagPopupScrollArea = nullptr;
    QWidget *m_tagPopupContent = nullptr;
    QGridLayout *m_tagPopupGrid = nullptr;
    QWidget *m_tagSelectedPanel = nullptr;
    QLayout *m_tagSelectedLayout = nullptr;
    QListWidget *m_todoList = nullptr;
    QScrollArea *m_todoScrollArea = nullptr;
    QVBoxLayout *m_todoContentLayout = nullptr;
    QBoxLayout *m_todoWorkspaceLayout = nullptr;
    QBoxLayout *m_todoMetaRow = nullptr;
    QBoxLayout *m_todoBottomRow = nullptr;
    QFrame *m_todoSidebarCard = nullptr;
    QFrame *m_todoEditorCard = nullptr;
    QWidget *m_todoStandardPanel = nullptr;
    QWidget *m_todoStandardHeaderPanel = nullptr;
    QWidget *m_todoCompactDatePanel = nullptr;
    QWidget *m_todoDateField = nullptr;
    QWidget *m_todoDueField = nullptr;

    QLabel *m_cycleHintLabel = nullptr;
    QLabel *m_pomodoroPhaseLabel = nullptr;
    QLabel *m_pomodoroTaskTimingLabel = nullptr;
    QSpinBox *m_focusMinutes = nullptr;
    QSpinBox *m_breakMinutes = nullptr;
    RoundedComboBox *m_pomodoroTaskSelector = nullptr;
    PomodoroDialWidget *m_pomodoroDial = nullptr;
    QListWidget *m_focusHistoryList = nullptr;
    QPushButton *m_presetDeepFocusButton = nullptr;
    QPushButton *m_presetBalancedButton = nullptr;
    QPushButton *m_presetQuickButton = nullptr;
    QPushButton *m_startPauseButton = nullptr;
    QPushButton *m_stopPomodoroButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_pomodoroFocusCardButton = nullptr;
    QPushButton *m_pomodoroTaskTimingButton = nullptr;
    PomodoroFocusCard *m_pomodoroFocusCard = nullptr;
    QWidget *m_pomodoroStandardPanel = nullptr;
    QWidget *m_pomodoroPresetPanel = nullptr;
    QWidget *m_pomodoroHistoryCard = nullptr;

    QLabel *m_totalTodayLabel = nullptr;
    QLabel *m_completedTodayLabel = nullptr;
    QLabel *m_completionRateLabel = nullptr;
    QLabel *m_focusTodayLabel = nullptr;
    QLabel *m_focusWeekLabel = nullptr;
    QLabel *m_taskTimingTodayLabel = nullptr;
    QLabel *m_activeTaskTimingLabel = nullptr;
    QLabel *m_statsHeroLabel = nullptr;
    QLabel *m_statsFocusInsightLabel = nullptr;
    QLabel *m_statsTimingInsightLabel = nullptr;
    QListWidget *m_statsRankingList = nullptr;
    TagAnalysisChartWidget *m_tagTimingChart = nullptr;
    TagAnalysisChartWidget *m_tagFocusChart = nullptr;
    QTimer *m_taskTimingRefreshTimer = nullptr;
    QVBoxLayout *m_statsContentLayout = nullptr;
    QGridLayout *m_statsOverviewGrid = nullptr;
    QGridLayout *m_statsDetailGrid = nullptr;
    QWidget *m_totalTodayCard = nullptr;
    QWidget *m_completedTodayCard = nullptr;
    QWidget *m_completionRateCard = nullptr;
    QWidget *m_focusTodayCard = nullptr;
    QWidget *m_focusWeekCard = nullptr;
    QWidget *m_taskTimingTodayCard = nullptr;
    QWidget *m_activeTaskTimingCard = nullptr;
    QWidget *m_statsStandardPanel = nullptr;
    QPushButton *m_statsRefreshButton = nullptr;

    QWidget *buildTodoTab();
    QWidget *buildPomodoroTab();
    QWidget *buildStatsTab();

    void refreshTodoList();
    void updateTodoItemSizeHints();
    void refreshStats();
    void refreshPomodoroView(int remainingSeconds, PomodoroTimer::Phase phase);
    void refreshPomodoroBindings();
    void refreshTagPresets();
    void refreshPomodoroTaskTimingPanel();
    void positionTagPopup();
    void updateDateFieldIndicators();
    void togglePomodoroFocusCard();
    void showPomodoroFocusCard();
    void hidePomodoroFocusCard(bool restoreMainWindow);
    void syncPomodoroFocusCard(const QString &phaseText, const QString &timeText, double progress);
    void updatePomodoroFocusCardButton();
    QPoint restoredMainWindowTopLeft() const;
    void toggleTodoTiming(const QString &id);
    void updateTaskTimingRefreshState();
    void applyWindowPresetForTab(int index);
    void setWindowLayoutMode(WindowLayoutMode mode);
    void applyWindowLayoutMode();
    void rebuildStatsMetricLayout();
    void loadUiState();
    void saveUiState() const;
    QString settingsFilePath() const;
    QString windowPositionSettingsKey(WindowLayoutMode mode) const;
    QPoint savedWindowPosition(WindowLayoutMode mode) const;
    void setupTrayIcon();
    void updateTrayActions();
    void showFromTray();
    void applyTheme();
    QWidget *createTodoCard(const TodoItem &todo);
    void refreshDateSidebar(const QList<TodoItem> &allTodos);
    void refreshCalendarHighlights(const QList<TodoItem> &allTodos);
    void populateTodoForm(const TodoItem &todo);
    void resetTodoForm();
    void refreshOverdueReminder(const QList<TodoItem> &todos);
    void updateTodoEditorState();
    bool deleteTodoById(const QString &id);
    bool matchesFilters(const TodoItem &todo) const;
    bool isOverdueToday(const TodoItem &todo) const;
    TodoListMode currentTodoListMode() const;
    bool isMinimalMode() const;
    QStringList collectTags() const;

    static QString formatSeconds(int totalSeconds);
    static QString formatDuration(qint64 totalSeconds);
    static QString phaseText(PomodoroTimer::Phase phase);
    static QString priorityText(TaskPriority priority);
    static QString todoListModeText(TodoListMode mode);
    static QString windowLayoutModeText(WindowLayoutMode mode);
    static QString formatDateTitle(const QDate &date);
    static QString formatTodoText(const TodoItem &todo);
};
