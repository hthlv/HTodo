#include "TaskStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

namespace {
TaskPriority priorityFromString(const QString &value) {
    if (value == "high") {
        return TaskPriority::High;
    }
    if (value == "low") {
        return TaskPriority::Low;
    }
    return TaskPriority::Medium;
}

QString priorityToString(TaskPriority priority) {
    switch (priority) {
    case TaskPriority::High:
        return "high";
    case TaskPriority::Low:
        return "low";
    case TaskPriority::Medium:
    default:
        return "medium";
    }
}

QString appDataDirPath() {
    const QString standardPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!standardPath.isEmpty()) {
        return standardPath;
    }

    const QString appDirPath = QCoreApplication::applicationDirPath();
    if (!appDirPath.isEmpty()) {
        return appDirPath;
    }

    return QDir::currentPath();
}

QString legacyDataFilePath() {
    QString dirPath = QCoreApplication::applicationDirPath();
    if (dirPath.isEmpty()) {
        dirPath = QDir::currentPath();
    }

    return QDir(dirPath).filePath("data.json");
}

bool ensureParentDir(const QString &filePath) {
    const QFileInfo fileInfo(filePath);
    QDir dir(fileInfo.absolutePath());
    return dir.exists() || dir.mkpath(".");
}
}

TaskStorage::TaskStorage() {
    m_filePath = QDir(appDataDirPath()).filePath("data.json");

    const QString legacyFilePath = legacyDataFilePath();
    if (legacyFilePath != m_filePath
        && QFileInfo::exists(legacyFilePath)
        && !QFileInfo::exists(m_filePath)
        && ensureStorageDir()) {
        QFile::copy(legacyFilePath, m_filePath);
    }

    load();

    bool hadActiveTiming = false;
    for (TodoItem &item : m_todos) {
        if (!item.timingStartedAt.isValid()) {
            continue;
        }

        item.timingStartedAt = QDateTime();
        hadActiveTiming = true;
    }

    if (hadActiveTiming) {
        save();
    }
}

bool TaskStorage::load() {
    m_todos.clear();
    m_tagCatalog = builtInTags();

    QFile file(m_filePath);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();

    const QJsonArray tagCatalogArray = root.value("tagCatalog").toArray();
    QStringList persistedTags;
    for (const QJsonValue &value : tagCatalogArray) {
        const QString tag = value.toString().trimmed();
        if (!tag.isEmpty()) {
            persistedTags.push_back(tag);
        }
    }
    m_tagCatalog = normalizeTags(m_tagCatalog + persistedTags);

    const QJsonArray todoArray = root.value("todos").toArray();
    for (const QJsonValue &value : todoArray) {
        const QJsonObject obj = value.toObject();

        TodoItem item;
        item.id = obj.value("id").toString();
        item.title = obj.value("title").toString();
        item.date = QDate::fromString(obj.value("date").toString(), Qt::ISODate);
        item.priority = priorityFromString(obj.value("priority").toString());
        item.completed = obj.value("completed").toBool(false);
        item.trackedSeconds = qMax<qint64>(0, obj.value("trackedSeconds").toVariant().toLongLong());

        const QString dueAtText = obj.value("dueAt").toString();
        if (!dueAtText.isEmpty()) {
            item.dueAt = QDateTime::fromString(dueAtText, Qt::ISODate);
        }

        const QJsonArray tagArray = obj.value("tags").toArray();
        QStringList tags;
        for (const QJsonValue &tagValue : tagArray) {
            const QString tag = tagValue.toString().trimmed();
            if (!tag.isEmpty()) {
                tags.push_back(tag);
            }
        }
        item.tags = normalizeTags(tags);

        const QString completedAtText = obj.value("completedAt").toString();
        if (!completedAtText.isEmpty()) {
            item.completedAt = QDateTime::fromString(completedAtText, Qt::ISODate);
        }

        const QString timingStartedAtText = obj.value("timingStartedAt").toString();
        if (!timingStartedAtText.isEmpty()) {
            item.timingStartedAt = QDateTime::fromString(timingStartedAtText, Qt::ISODate);
        }

        const QJsonArray focusArray = obj.value("focusRecords").toArray();
        for (const QJsonValue &focusValue : focusArray) {
            const QJsonObject focusObj = focusValue.toObject();
            TodoItem::FocusRecord record;
            record.completedAt = QDateTime::fromString(focusObj.value("completedAt").toString(), Qt::ISODate);
            record.focusMinutes = focusObj.value("focusMinutes").toInt(0);
            if (record.completedAt.isValid()) {
                item.focusRecords.push_back(record);
            }
        }

        if (!item.id.isEmpty() && !item.title.trimmed().isEmpty() && item.date.isValid()) {
            m_todos.push_back(item);
        }
    }

    const QJsonArray legacyFocusArray = root.value("focusSessions").toArray();
    for (const QJsonValue &value : legacyFocusArray) {
        QDateTime completedAt;
        QString taskId;
        int focusMinutes = 0;

        if (value.isString()) {
            completedAt = QDateTime::fromString(value.toString(), Qt::ISODate);
        } else {
            const QJsonObject obj = value.toObject();
            completedAt = QDateTime::fromString(obj.value("completedAt").toString(), Qt::ISODate);
            taskId = obj.value("taskId").toString();
            focusMinutes = obj.value("focusMinutes").toInt(0);
        }

        if (!completedAt.isValid() || taskId.isEmpty()) {
            continue;
        }

        for (TodoItem &item : m_todos) {
            if (item.id != taskId) {
                continue;
            }

            TodoItem::FocusRecord record;
            record.completedAt = completedAt;
            record.focusMinutes = focusMinutes;
            item.focusRecords.push_back(record);
            break;
        }
    }

    rebuildTagCatalog();

    return true;
}

bool TaskStorage::save() const {
    if (!ensureStorageDir()) {
        return false;
    }

    QJsonArray todoArray;
    for (const TodoItem &item : m_todos) {
        QJsonObject obj;
        obj.insert("id", item.id);
        obj.insert("title", item.title);
        obj.insert("date", item.date.toString(Qt::ISODate));
        obj.insert("priority", priorityToString(item.priority));
        obj.insert("dueAt", item.dueAt.isValid() ? item.dueAt.toString(Qt::ISODate) : QString());

        QJsonArray tagArray;
        for (const QString &tag : item.tags) {
            tagArray.push_back(tag);
        }
        obj.insert("tags", tagArray);

        obj.insert("completed", item.completed);
        obj.insert("completedAt", item.completedAt.isValid() ? item.completedAt.toString(Qt::ISODate) : QString());
        obj.insert("trackedSeconds", static_cast<qint64>(qMax<qint64>(0, item.trackedSeconds)));
        obj.insert("timingStartedAt", item.timingStartedAt.isValid() ? item.timingStartedAt.toString(Qt::ISODate) : QString());

        QJsonArray focusArray;
        for (const TodoItem::FocusRecord &record : item.focusRecords) {
            QJsonObject focusObj;
            focusObj.insert("completedAt", record.completedAt.toString(Qt::ISODate));
            focusObj.insert("focusMinutes", record.focusMinutes);
            focusArray.push_back(focusObj);
        }
        obj.insert("focusRecords", focusArray);
        todoArray.push_back(obj);
    }

    QJsonObject root;
    root.insert("todos", todoArray);

    QJsonArray tagCatalogArray;
    for (const QString &tag : m_tagCatalog) {
        tagCatalogArray.push_back(tag);
    }
    root.insert("tagCatalog", tagCatalogArray);

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QList<TodoItem> TaskStorage::allTodos() const {
    return m_todos;
}

QStringList TaskStorage::availableTags() const {
    return m_tagCatalog;
}

QList<TodoItem> TaskStorage::todosForDate(const QDate &date) const {
    QList<TodoItem> result;
    for (const TodoItem &item : m_todos) {
        if (item.date == date) {
            result.push_back(item);
        }
    }
    return result;
}

std::optional<TodoItem> TaskStorage::todoById(const QString &id) const {
    for (const TodoItem &item : m_todos) {
        if (item.id == id) {
            return item;
        }
    }

    return std::nullopt;
}

bool TaskStorage::addTodo(const QString &title,
                         const QDate &date,
                         TaskPriority priority,
                         const QDateTime &dueAt,
                         const QStringList &tags) {
    const QString cleaned = title.trimmed();
    if (cleaned.isEmpty() || !date.isValid()) {
        return false;
    }

    TodoItem item;
    item.id = generateId();
    item.title = cleaned;
    item.date = date;
    item.priority = priority;
    item.dueAt = dueAt;
    item.tags = normalizeTags(tags);
    item.completed = false;

    m_todos.push_back(item);
    rebuildTagCatalog();
    if (save()) {
        return true;
    }

    m_todos.removeLast();
    rebuildTagCatalog();
    return false;
}

bool TaskStorage::updateTodo(const TodoItem &todo) {
    if (todo.id.isEmpty() || todo.title.trimmed().isEmpty() || !todo.date.isValid()) {
        return false;
    }

    for (int i = 0; i < m_todos.size(); ++i) {
        if (m_todos[i].id == todo.id) {
            const TodoItem oldTodo = m_todos[i];
            TodoItem updatedTodo = todo;
            updatedTodo.title = updatedTodo.title.trimmed();
            updatedTodo.tags = normalizeTags(updatedTodo.tags);

            m_todos[i] = updatedTodo;
            rebuildTagCatalog();
            if (save()) {
                return true;
            }

            m_todos[i] = oldTodo;
            rebuildTagCatalog();
            return false;
        }
    }

    return false;
}

bool TaskStorage::removeTodo(const QString &id) {
    for (int i = 0; i < m_todos.size(); ++i) {
        if (m_todos[i].id == id) {
            const TodoItem removed = m_todos.takeAt(i);
            rebuildTagCatalog();
            if (save()) {
                return true;
            }

            m_todos.insert(i, removed);
            rebuildTagCatalog();
            return false;
        }
    }

    return false;
}

bool TaskStorage::setTodoCompleted(const QString &id, bool completed) {
    for (TodoItem &item : m_todos) {
        if (item.id == id) {
            const bool oldCompleted = item.completed;
            const QDateTime oldCompletedAt = item.completedAt;
            const qint64 oldTrackedSeconds = item.trackedSeconds;
            const QDateTime oldTimingStartedAt = item.timingStartedAt;

            item.completed = completed;
            item.completedAt = completed ? QDateTime::currentDateTime() : QDateTime();
            if (completed && item.timingStartedAt.isValid()) {
                finalizeTimingSession(item, QDateTime::currentDateTime());
            }
            if (save()) {
                return true;
            }

            item.completed = oldCompleted;
            item.completedAt = oldCompletedAt;
            item.trackedSeconds = oldTrackedSeconds;
            item.timingStartedAt = oldTimingStartedAt;
            return false;
        }
    }

    return false;
}

bool TaskStorage::startTodoTiming(const QString &id) {
    int targetIndex = -1;
    int activeIndex = -1;
    for (int i = 0; i < m_todos.size(); ++i) {
        if (m_todos[i].id == id) {
            targetIndex = i;
        }
        if (m_todos[i].timingStartedAt.isValid()) {
            activeIndex = i;
        }
    }

    if (targetIndex < 0 || m_todos[targetIndex].completed) {
        return false;
    }

    if (activeIndex >= 0 && activeIndex != targetIndex) {
        return false;
    }

    if (m_todos[targetIndex].timingStartedAt.isValid()) {
        return true;
    }

    const QDateTime now = QDateTime::currentDateTime();
    m_todos[targetIndex].timingStartedAt = now;
    if (save()) {
        return true;
    }

    m_todos[targetIndex].timingStartedAt = QDateTime();
    return false;
}

bool TaskStorage::stopTodoTiming(const QString &id) {
    for (TodoItem &item : m_todos) {
        if (item.id != id) {
            continue;
        }

        if (!item.timingStartedAt.isValid()) {
            return true;
        }

        const qint64 oldTrackedSeconds = item.trackedSeconds;
        const QDateTime oldTimingStartedAt = item.timingStartedAt;
        finalizeTimingSession(item, QDateTime::currentDateTime());
        if (save()) {
            return true;
        }

        item.trackedSeconds = oldTrackedSeconds;
        item.timingStartedAt = oldTimingStartedAt;
        return false;
    }

    return false;
}

bool TaskStorage::addTrackedSeconds(const QString &id, qint64 seconds) {
    if (id.isEmpty() || seconds <= 0) {
        return false;
    }

    for (TodoItem &item : m_todos) {
        if (item.id != id) {
            continue;
        }

        const qint64 oldTrackedSeconds = item.trackedSeconds;
        item.trackedSeconds += seconds;
        if (save()) {
            return true;
        }

        item.trackedSeconds = oldTrackedSeconds;
        return false;
    }

    return false;
}

bool TaskStorage::hasActiveTodoTiming() const {
    for (const TodoItem &item : m_todos) {
        if (item.timingStartedAt.isValid()) {
            return true;
        }
    }

    return false;
}

std::optional<TodoItem> TaskStorage::activeTimedTodo() const {
    for (const TodoItem &item : m_todos) {
        if (item.timingStartedAt.isValid()) {
            return item;
        }
    }

    return std::nullopt;
}

bool TaskStorage::addFocusRecord(const QString &todoId, const QDateTime &completedAt, int focusMinutes) {
    if (todoId.isEmpty() || !completedAt.isValid()) {
        return false;
    }

    for (TodoItem &item : m_todos) {
        if (item.id != todoId) {
            continue;
        }

        TodoItem::FocusRecord record;
        record.completedAt = completedAt;
        record.focusMinutes = focusMinutes;
        item.focusRecords.push_back(record);
        if (save()) {
            return true;
        }

        item.focusRecords.removeLast();
        return false;
    }

    return false;
}

QList<TodoItem::FocusRecord> TaskStorage::focusRecordsForDate(const QDate &date) const {
    QList<TodoItem::FocusRecord> result;
    for (const TodoItem &item : m_todos) {
        for (const TodoItem::FocusRecord &record : item.focusRecords) {
            if (record.completedAt.date() == date) {
                result.push_back(record);
            }
        }
    }

    return result;
}

int TaskStorage::focusCountForDate(const QDate &date) const {
    int count = 0;
    for (const TodoItem &item : m_todos) {
        for (const TodoItem::FocusRecord &record : item.focusRecords) {
            if (record.completedAt.date() == date) {
                ++count;
            }
        }
    }

    return count;
}

int TaskStorage::focusCountForRecentDays(const QDate &endDate, int days) const {
    if (days <= 0 || !endDate.isValid()) {
        return 0;
    }

    const QDate startDate = endDate.addDays(-(days - 1));
    int count = 0;
    for (const TodoItem &item : m_todos) {
        for (const TodoItem::FocusRecord &record : item.focusRecords) {
            const QDate d = record.completedAt.date();
            if (d >= startDate && d <= endDate) {
                ++count;
            }
        }
    }

    return count;
}

TodoStats TaskStorage::currentStats(const QDate &date) const {
    TodoStats stats;
    const QList<TodoItem> dailyTodos = todosForDate(date);
    const QDateTime now = QDateTime::currentDateTime();

    stats.totalToday = dailyTodos.size();
    for (const TodoItem &item : dailyTodos) {
        if (item.completed) {
            ++stats.completedToday;
        }
        stats.trackedSecondsToday += effectiveTrackedSeconds(item, now);
    }

    for (const TodoItem &item : m_todos) {
        stats.trackedSecondsAll += effectiveTrackedSeconds(item, now);
    }

    stats.focusToday = focusCountForDate(date);
    stats.focusLast7Days = focusCountForRecentDays(date, 7);
    return stats;
}

qint64 TaskStorage::effectiveTrackedSeconds(const TodoItem &todo, const QDateTime &referenceTime) {
    qint64 total = qMax<qint64>(0, todo.trackedSeconds);
    if (todo.timingStartedAt.isValid() && referenceTime.isValid()) {
        total += qMax<qint64>(0, todo.timingStartedAt.secsTo(referenceTime));
    }
    return total;
}

QString TaskStorage::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool TaskStorage::ensureStorageDir() const {
    return ensureParentDir(m_filePath);
}

QStringList TaskStorage::normalizeTags(const QStringList &tags) {
    QStringList normalized;

    for (const QString &rawTag : tags) {
        const QString tag = rawTag.trimmed();
        if (tag.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const QString &existing : normalized) {
            if (QString::compare(existing, tag, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            normalized.push_back(tag);
        }
    }

    return normalized;
}

QStringList TaskStorage::builtInTags() {
    return {"工作", "学习", "生活", "紧急", "重要", "长期"};
}

void TaskStorage::rebuildTagCatalog() {
    QStringList combined = builtInTags();
    for (const TodoItem &item : m_todos) {
        combined.append(item.tags);
    }
    combined.append(m_tagCatalog);
    m_tagCatalog = normalizeTags(combined);
}

void TaskStorage::finalizeTimingSession(TodoItem &todo, const QDateTime &endedAt) {
    if (!todo.timingStartedAt.isValid() || !endedAt.isValid()) {
        return;
    }

    todo.trackedSeconds = effectiveTrackedSeconds(todo, endedAt);
    todo.timingStartedAt = QDateTime();
}
