#include "TaskStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
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

QString storageSettingsFilePath() {
    const QString appName = QCoreApplication::applicationName().isEmpty()
                                ? QStringLiteral("HTodo")
                                : QCoreApplication::applicationName();
    const QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString dirPath = configPath.isEmpty() ? appDataDirPath() : configPath;
    return QDir(dirPath).filePath(appName + ".ini");
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

QDate todoDisplayEndDate(const TodoItem &item) {
    if (!item.date.isValid()) {
        return {};
    }

    if (item.dueAt.isValid()) {
        const QDate dueDate = item.dueAt.date();
        if (dueDate.isValid() && dueDate > item.date) {
            return dueDate;
        }
    }

    return item.date;
}

bool todoCoversDate(const TodoItem &item, const QDate &date) {
    if (!date.isValid() || !item.date.isValid()) {
        return false;
    }

    return date >= item.date && date <= todoDisplayEndDate(item);
}
}

TaskStorage::TaskStorage() {
    QString initialPath = QDir(appDataDirPath()).filePath("data.json");
#if defined(Q_OS_WINDOWS)
    QSettings settings(storageSettingsFilePath(), QSettings::IniFormat);
    const QString customPath = settings.value("storage/data_file_path").toString().trimmed();
    if (!customPath.isEmpty()) {
        initialPath = customPath;
    }
#endif
    m_filePath = initialPath;

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

    ensureDailyPlansGenerated(QDate::currentDate());
}

bool TaskStorage::load() {
    m_todos.clear();
    m_plans.clear();
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
        item.sourcePlanId = obj.value("sourcePlanId").toString();
        item.autoGeneratedFromPlan = obj.value("autoGeneratedFromPlan").toBool(false);

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

    const QJsonArray planArray = root.value("plans").toArray();
    for (const QJsonValue &value : planArray) {
        const QJsonObject obj = value.toObject();
        TodoPlan plan;
        plan.id = obj.value("id").toString();
        plan.title = obj.value("title").toString().trimmed();
        plan.startDate = QDate::fromString(obj.value("startDate").toString(), Qt::ISODate);
        plan.endDate = QDate::fromString(obj.value("endDate").toString(), Qt::ISODate);
        plan.priority = priorityFromString(obj.value("priority").toString());
        plan.paused = obj.value("paused").toBool(false);
        plan.lastGeneratedDate = QDate::fromString(obj.value("lastGeneratedDate").toString(), Qt::ISODate);
        plan.tags = normalizeTags(obj.value("tags").toVariant().toStringList());

        const QString dueTimeText = obj.value("dueTime").toString();
        if (!dueTimeText.isEmpty()) {
            plan.dueTime = QTime::fromString(dueTimeText, "HH:mm:ss");
            if (!plan.dueTime.isValid()) {
                plan.dueTime = QTime::fromString(dueTimeText, "HH:mm");
            }
        }

        if (!plan.id.isEmpty() && !plan.title.isEmpty() && plan.startDate.isValid()
            && plan.endDate.isValid() && plan.endDate >= plan.startDate) {
            m_plans.push_back(plan);
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
        obj.insert("sourcePlanId", item.sourcePlanId);
        obj.insert("autoGeneratedFromPlan", item.autoGeneratedFromPlan);

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

    QJsonArray planArray;
    for (const TodoPlan &plan : m_plans) {
        QJsonObject obj;
        obj.insert("id", plan.id);
        obj.insert("title", plan.title);
        obj.insert("startDate", plan.startDate.toString(Qt::ISODate));
        obj.insert("endDate", plan.endDate.toString(Qt::ISODate));
        obj.insert("priority", priorityToString(plan.priority));
        obj.insert("dueTime", plan.dueTime.isValid() ? plan.dueTime.toString("HH:mm:ss") : QString());
        obj.insert("paused", plan.paused);
        obj.insert("lastGeneratedDate", plan.lastGeneratedDate.isValid() ? plan.lastGeneratedDate.toString(Qt::ISODate) : QString());

        QJsonArray tagArray;
        for (const QString &tag : plan.tags) {
            tagArray.push_back(tag);
        }
        obj.insert("tags", tagArray);
        planArray.push_back(obj);
    }
    root.insert("plans", planArray);

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

QString TaskStorage::storageFilePath() const {
    return m_filePath;
}

bool TaskStorage::setStorageFilePath(const QString &filePath) {
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QString normalized = QFileInfo(trimmed).absoluteFilePath();
    if (normalized == m_filePath) {
        return true;
    }

    const QString oldFilePath = m_filePath;
    m_filePath = normalized;
    const bool saved = save();
    if (!saved) {
        m_filePath = oldFilePath;
        return false;
    }

#if defined(Q_OS_WINDOWS)
    if (ensureParentDir(storageSettingsFilePath())) {
        QSettings settings(storageSettingsFilePath(), QSettings::IniFormat);
        settings.setValue("storage/data_file_path", m_filePath);
        settings.sync();
    }
#endif
    return true;
}

QList<TodoItem> TaskStorage::allTodos() const {
    return m_todos;
}

QList<TodoPlan> TaskStorage::allPlans() const {
    return m_plans;
}

QStringList TaskStorage::availableTags() const {
    return m_tagCatalog;
}

QList<TodoItem> TaskStorage::todosForDate(const QDate &date) const {
    QList<TodoItem> result;
    for (const TodoItem &item : m_todos) {
        if (todoCoversDate(item, date)) {
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

std::optional<TodoPlan> TaskStorage::planById(const QString &id) const {
    for (const TodoPlan &plan : m_plans) {
        if (plan.id == id) {
            return plan;
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

bool TaskStorage::addDailyPlan(const QString &title,
                               const QDate &startDate,
                               const QDate &endDate,
                               TaskPriority priority,
                               const QDateTime &dueAtTemplate,
                               const QStringList &tags,
                               int *generatedCount) {
    const QString cleaned = title.trimmed();
    if (cleaned.isEmpty() || !startDate.isValid() || !endDate.isValid() || endDate < startDate) {
        if (generatedCount != nullptr) {
            *generatedCount = 0;
        }
        return false;
    }

    TodoPlan plan;
    plan.id = generateId();
    plan.title = cleaned;
    plan.startDate = startDate;
    plan.endDate = endDate;
    plan.priority = priority;
    plan.tags = normalizeTags(tags);
    plan.paused = false;
    if (dueAtTemplate.isValid()) {
        plan.dueTime = dueAtTemplate.time();
    }

    m_plans.push_back(plan);
    const int planIndex = m_plans.size() - 1;
    int generated = 0;
    const bool generatedOk = generatePlanTodosUntil(m_plans[planIndex], endDate, &generated);
    rebuildTagCatalog();

    if (generatedOk && save()) {
        if (generatedCount != nullptr) {
            *generatedCount = generated;
        }
        return true;
    }

    m_plans.removeAt(planIndex);
    for (int i = m_todos.size() - 1; i >= 0; --i) {
        if (m_todos[i].sourcePlanId == plan.id && m_todos[i].autoGeneratedFromPlan) {
            m_todos.removeAt(i);
        }
    }
    rebuildTagCatalog();
    if (generatedCount != nullptr) {
        *generatedCount = 0;
    }
    return false;
}

bool TaskStorage::updatePlan(const TodoPlan &plan, bool syncPastUnfinished) {
    if (plan.id.isEmpty() || plan.title.trimmed().isEmpty()
        || !plan.startDate.isValid() || !plan.endDate.isValid()
        || plan.endDate < plan.startDate) {
        return false;
    }

    for (int i = 0; i < m_plans.size(); ++i) {
        if (m_plans[i].id != plan.id) {
            continue;
        }

        const TodoPlan oldPlan = m_plans[i];
        const QList<TodoItem> oldTodos = m_todos;
        const QStringList oldTagCatalog = m_tagCatalog;

        TodoPlan updated = plan;
        updated.title = updated.title.trimmed();
        updated.tags = normalizeTags(updated.tags);

        // Keep generation cursor stable; if date range is moved earlier, restart from new start.
        if (!updated.lastGeneratedDate.isValid()) {
            updated.lastGeneratedDate = oldPlan.lastGeneratedDate;
        }
        if (updated.lastGeneratedDate.isValid() && updated.lastGeneratedDate < updated.startDate) {
            updated.lastGeneratedDate = updated.startDate.addDays(-1);
        }
        if (updated.lastGeneratedDate.isValid() && updated.lastGeneratedDate > updated.endDate) {
            updated.lastGeneratedDate = updated.endDate;
        }

        m_plans[i] = updated;

        // Remove auto-generated todos that are now outside the new range.
        for (int t = m_todos.size() - 1; t >= 0; --t) {
            const TodoItem &item = m_todos[t];
            if (item.sourcePlanId != updated.id || !item.autoGeneratedFromPlan) {
                continue;
            }
            if (item.date < updated.startDate || item.date > updated.endDate) {
                m_todos.removeAt(t);
            }
        }

        // Sync future, unfinished, auto-generated todos with the updated plan definition.
        const QDate today = QDate::currentDate();
        for (TodoItem &item : m_todos) {
            if (item.sourcePlanId != updated.id || !item.autoGeneratedFromPlan) {
                continue;
            }
            if (item.completed) {
                continue;
            }
            if (!syncPastUnfinished && item.date < today) {
                continue;
            }
            item.title = updated.title;
            item.priority = updated.priority;
            item.tags = updated.tags;
            item.dueAt = updated.dueTime.isValid() ? QDateTime(item.date, updated.dueTime) : QDateTime();
        }

        // Keep the whole plan range materialized so future dates are visible immediately.
        generatePlanTodosUntil(m_plans[i], m_plans[i].endDate, nullptr);

        rebuildTagCatalog();
        if (save()) {
            return true;
        }

        m_plans[i] = oldPlan;
        m_todos = oldTodos;
        m_tagCatalog = oldTagCatalog;
        return false;
    }

    return false;
}

bool TaskStorage::addDailyTodoPlan(const QString &title,
                                   const QDate &startDate,
                                   const QDate &endDate,
                                   TaskPriority priority,
                                   const QDateTime &dueAtTemplate,
                                   const QStringList &tags,
                                   int *createdCount) {
    const QString cleaned = title.trimmed();
    if (cleaned.isEmpty() || !startDate.isValid() || !endDate.isValid() || endDate < startDate) {
        if (createdCount != nullptr) {
            *createdCount = 0;
        }
        return false;
    }

    QList<TodoItem> plannedTodos;
    for (QDate date = startDate; date <= endDate; date = date.addDays(1)) {
        TodoItem item;
        item.id = generateId();
        item.title = cleaned;
        item.date = date;
        item.priority = priority;
        item.tags = normalizeTags(tags);
        item.completed = false;

        if (dueAtTemplate.isValid()) {
            const QTime dueTime = dueAtTemplate.time();
            item.dueAt = QDateTime(date, dueTime);
        }

        plannedTodos.push_back(item);
    }

    if (plannedTodos.isEmpty()) {
        if (createdCount != nullptr) {
            *createdCount = 0;
        }
        return false;
    }

    const int oldCount = m_todos.size();
    for (const TodoItem &item : plannedTodos) {
        m_todos.push_back(item);
    }
    rebuildTagCatalog();
    if (save()) {
        if (createdCount != nullptr) {
            *createdCount = plannedTodos.size();
        }
        return true;
    }

    while (m_todos.size() > oldCount) {
        m_todos.removeLast();
    }
    rebuildTagCatalog();
    if (createdCount != nullptr) {
        *createdCount = 0;
    }
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

bool TaskStorage::removeTag(const QString &tag) {
    const QString cleanedTag = tag.trimmed();
    if (cleanedTag.isEmpty()) {
        return false;
    }

    const QList<TodoItem> oldTodos = m_todos;
    const QStringList oldTagCatalog = m_tagCatalog;
    bool changed = false;

    for (TodoItem &item : m_todos) {
        QStringList updatedTags;
        for (const QString &existing : item.tags) {
            if (QString::compare(existing, cleanedTag, Qt::CaseInsensitive) == 0) {
                changed = true;
                continue;
            }
            updatedTags.push_back(existing);
        }
        item.tags = updatedTags;
    }

    QStringList updatedCatalog;
    for (const QString &existing : m_tagCatalog) {
        if (QString::compare(existing, cleanedTag, Qt::CaseInsensitive) == 0) {
            changed = true;
            continue;
        }
        updatedCatalog.push_back(existing);
    }
    m_tagCatalog = updatedCatalog;
    rebuildTagCatalog();

    if (!changed) {
        return false;
    }

    if (save()) {
        return true;
    }

    m_todos = oldTodos;
    m_tagCatalog = oldTagCatalog;
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

bool TaskStorage::setPlanPaused(const QString &planId, bool paused) {
    for (TodoPlan &plan : m_plans) {
        if (plan.id != planId) {
            continue;
        }
        const bool oldPaused = plan.paused;
        const QDate oldLastGeneratedDate = plan.lastGeneratedDate;
        plan.paused = paused;

        bool ok = true;
        if (!paused) {
            ok = generatePlanTodosUntil(plan, plan.endDate, nullptr);
        }

        rebuildTagCatalog();
        if (ok && save()) {
            return true;
        }

        plan.paused = oldPaused;
        plan.lastGeneratedDate = oldLastGeneratedDate;
        rebuildTagCatalog();
        return false;
    }
    return false;
}

bool TaskStorage::removePlan(const QString &planId) {
    for (int i = 0; i < m_plans.size(); ++i) {
        if (m_plans[i].id != planId) {
            continue;
        }

        const TodoPlan removedPlan = m_plans.takeAt(i);
        QList<TodoItem> removedTodos;
        for (int j = m_todos.size() - 1; j >= 0; --j) {
            if (m_todos[j].sourcePlanId == planId && m_todos[j].autoGeneratedFromPlan) {
                removedTodos.push_back(m_todos.takeAt(j));
            }
        }

        rebuildTagCatalog();
        if (save()) {
            return true;
        }

        m_plans.insert(i, removedPlan);
        for (const TodoItem &item : removedTodos) {
            m_todos.push_back(item);
        }
        rebuildTagCatalog();
        return false;
    }

    return false;
}

int TaskStorage::ensureDailyPlansGenerated(const QDate &untilDate) {
    if (!untilDate.isValid()) {
        return 0;
    }

    int generatedTotal = 0;
    bool changed = false;
    for (TodoPlan &plan : m_plans) {
        int generated = 0;
        const QDate targetDate = plan.endDate.isValid() ? plan.endDate : untilDate;
        if (generatePlanTodosUntil(plan, targetDate, &generated)) {
            if (generated > 0) {
                changed = true;
                generatedTotal += generated;
            }
        }
    }

    if (changed) {
        rebuildTagCatalog();
        save();
    }

    return generatedTotal;
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

bool TaskStorage::generatePlanTodosUntil(TodoPlan &plan, const QDate &untilDate, int *generatedCount) {
    if (generatedCount != nullptr) {
        *generatedCount = 0;
    }
    if (plan.paused || !plan.startDate.isValid() || !plan.endDate.isValid() || !untilDate.isValid()) {
        return true;
    }

    const QDate targetDate = qMin(untilDate, plan.endDate);
    if (targetDate < plan.startDate) {
        return true;
    }

    QDate cursor = plan.lastGeneratedDate.isValid() ? plan.lastGeneratedDate.addDays(1) : plan.startDate;
    if (cursor < plan.startDate) {
        cursor = plan.startDate;
    }

    int generated = 0;
    while (cursor.isValid() && cursor <= targetDate) {
        if (!hasPlanTodoOnDate(plan.id, cursor)) {
            TodoItem item;
            item.id = generateId();
            item.title = plan.title;
            item.date = cursor;
            item.priority = plan.priority;
            item.tags = plan.tags;
            item.completed = false;
            item.sourcePlanId = plan.id;
            item.autoGeneratedFromPlan = true;
            if (plan.dueTime.isValid()) {
                item.dueAt = QDateTime(cursor, plan.dueTime);
            }
            m_todos.push_back(item);
            ++generated;
        }
        plan.lastGeneratedDate = cursor;
        cursor = cursor.addDays(1);
    }

    if (generatedCount != nullptr) {
        *generatedCount = generated;
    }
    return true;
}

bool TaskStorage::hasPlanTodoOnDate(const QString &planId, const QDate &date) const {
    for (const TodoItem &item : m_todos) {
        if (item.sourcePlanId == planId && item.date == date) {
            return true;
        }
    }
    return false;
}

void TaskStorage::rebuildTagCatalog() {
    QStringList combined = builtInTags();
    for (const TodoPlan &plan : m_plans) {
        combined.append(plan.tags);
    }
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
