#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <optional>

enum class TaskPriority {
    Low = 0,
    Medium = 1,
    High = 2
};

struct TodoItem {
    struct FocusRecord {
        QDateTime completedAt;
        int focusMinutes = 0;
    };

    QString id;
    QString title;
    QDate date;
    TaskPriority priority = TaskPriority::Medium;
    QDateTime dueAt;
    QStringList tags;
    bool completed = false;
    QDateTime completedAt;
    qint64 trackedSeconds = 0;
    QDateTime timingStartedAt;
    QList<FocusRecord> focusRecords;
};

struct TodoStats {
    int totalToday = 0;
    int completedToday = 0;
    int focusToday = 0;
    int focusLast7Days = 0;
    qint64 trackedSecondsToday = 0;
    qint64 trackedSecondsAll = 0;
};

class TaskStorage {
public:
    TaskStorage();

    bool load();
    bool save() const;

    QList<TodoItem> allTodos() const;
    QStringList availableTags() const;
    QList<TodoItem> todosForDate(const QDate &date) const;
    std::optional<TodoItem> todoById(const QString &id) const;
    bool addTodo(const QString &title,
                 const QDate &date,
                 TaskPriority priority,
                 const QDateTime &dueAt,
                 const QStringList &tags);
    bool updateTodo(const TodoItem &todo);
    bool removeTodo(const QString &id);
    bool setTodoCompleted(const QString &id, bool completed);
    bool startTodoTiming(const QString &id);
    bool stopTodoTiming(const QString &id);
    bool addTrackedSeconds(const QString &id, qint64 seconds);
    bool hasActiveTodoTiming() const;
    std::optional<TodoItem> activeTimedTodo() const;

    bool addFocusRecord(const QString &todoId, const QDateTime &completedAt, int focusMinutes);
    QList<TodoItem::FocusRecord> focusRecordsForDate(const QDate &date) const;
    int focusCountForDate(const QDate &date) const;
    int focusCountForRecentDays(const QDate &endDate, int days) const;

    TodoStats currentStats(const QDate &date) const;
    static qint64 effectiveTrackedSeconds(const TodoItem &todo, const QDateTime &referenceTime);

private:
    QString m_filePath;
    QList<TodoItem> m_todos;
    QStringList m_tagCatalog;

    static QString generateId();
    bool ensureStorageDir() const;
    static QStringList normalizeTags(const QStringList &tags);
    static void finalizeTimingSession(TodoItem &todo, const QDateTime &endedAt);
    static QStringList builtInTags();
    void rebuildTagCatalog();
};
