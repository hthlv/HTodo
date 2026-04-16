#include "MainWindow.h"
#include "RoundedComboBox.h"
#include "TagSelectorWidget.h"

#include <algorithm>
#include <QAbstractItemView>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFont>
#include <QFontMetrics>
#include <functional>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QPushButton>
#include <QProgressBar>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QTextCharFormat>
#include <QTimeEdit>
#include <QAbstractSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

namespace {
enum class AppDialogKind {
    Info,
    Warning,
    Confirm
};

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent = nullptr, int margin = 0, int hSpacing = 8, int vSpacing = 8)
        : QLayout(parent), m_hSpacing(hSpacing), m_vSpacing(vSpacing) {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override {
        QLayoutItem *item = nullptr;
        while ((item = takeAt(0)) != nullptr) {
            delete item;
        }
    }

    void addItem(QLayoutItem *item) override {
        m_items.append(item);
    }

    int count() const override {
        return m_items.size();
    }

    QLayoutItem *itemAt(int index) const override {
        return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
    }

    QLayoutItem *takeAt(int index) override {
        return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
    }

    Qt::Orientations expandingDirections() const override {
        return {};
    }

    bool hasHeightForWidth() const override {
        return true;
    }

    int heightForWidth(int width) const override {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize sizeHint() const override {
        return minimumSize();
    }

    QSize minimumSize() const override {
        QSize size;
        for (const QLayoutItem *item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    void setGeometry(const QRect &rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const {
        const QMargins margins = contentsMargins();
        QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;

        for (QLayoutItem *item : m_items) {
            const QSize itemSize = item->sizeHint();
            const int nextX = x + itemSize.width() + m_hSpacing;
            if (nextX - m_hSpacing > effectiveRect.right() + 1 && lineHeight > 0) {
                x = effectiveRect.x();
                y += lineHeight + m_vSpacing;
                lineHeight = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), itemSize));
            }

            x += itemSize.width() + m_hSpacing;
            lineHeight = qMax(lineHeight, itemSize.height());
        }

        return y + lineHeight - rect.y() + margins.bottom();
    }

    QList<QLayoutItem *> m_items;
    int m_hSpacing;
    int m_vSpacing;
};

class WheelBlockerFilter : public QObject {
public:
    explicit WheelBlockerFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        Q_UNUSED(watched);
        return event != nullptr && event->type() == QEvent::Wheel;
    }
};

constexpr int kBaseWindowWidth = 560;
constexpr int kBaseWindowHeight = 860;
constexpr double kTargetScreenWidthRatio = 0.92;
constexpr double kTargetScreenHeightRatio = 0.82;

QIcon createAppIcon() {
#if defined(Q_OS_LINUX)
    const QIcon themedIcon = QIcon::fromTheme(QStringLiteral("htodo"));
    if (!themedIcon.isNull()) {
        return themedIcon;
    }
    const QIcon svgIcon(QStringLiteral(":/icons/htodo.svg"));
    if (!svgIcon.isNull()) {
        return svgIcon;
    }
#endif
    return QIcon(QStringLiteral(":/icons/htodo.png"));
}

QRect availableGeometryForWindow(const QWidget *window) {
    if (window != nullptr) {
        if (QScreen *screenHandle = window->screen(); screenHandle != nullptr) {
            return screenHandle->availableGeometry();
        }
    }

    if (QScreen *screenHandle = QGuiApplication::primaryScreen(); screenHandle != nullptr) {
        return screenHandle->availableGeometry();
    }

    return QRect(0, 0, kBaseWindowWidth, kBaseWindowHeight);
}

QRect availableGeometryForScreen(const QScreen *screenHandle) {
    if (screenHandle != nullptr) {
        return screenHandle->availableGeometry();
    }
    return availableGeometryForWindow(nullptr);
}

QSize fixedWindowSizeForGeometry(const QRect &availableGeometry) {
    if (!availableGeometry.isValid()) {
        return QSize(kBaseWindowWidth, kBaseWindowHeight);
    }

    const double widthScale =
        static_cast<double>(availableGeometry.width()) * kTargetScreenWidthRatio / kBaseWindowWidth;
    const double heightScale =
        static_cast<double>(availableGeometry.height()) * kTargetScreenHeightRatio / kBaseWindowHeight;
    const double scale = qMin(1.0, qMin(widthScale, heightScale));

    if (scale <= 0.0) {
        return QSize(kBaseWindowWidth, kBaseWindowHeight);
    }

    return QSize(
        qMax(1, qRound(kBaseWindowWidth * scale)),
        qMax(1, qRound(kBaseWindowHeight * scale)));
}

QSize boundedWindowSize(const QRect &availableGeometry, QSize requestedSize, const QSize &minimumSize) {
    requestedSize = requestedSize.expandedTo(minimumSize);
    if (availableGeometry.isValid()) {
        requestedSize.setWidth(qMin(requestedSize.width(), availableGeometry.width()));
        requestedSize.setHeight(qMin(requestedSize.height(), availableGeometry.height()));
    }
    requestedSize.setWidth(qMax(1, requestedSize.width()));
    requestedSize.setHeight(qMax(1, requestedSize.height()));
    return requestedSize;
}

QPoint centeredTopLeft(const QRect &availableGeometry, const QSize &windowSize) {
    if (!availableGeometry.isValid()) {
        return {};
    }

    return QPoint(
        availableGeometry.center().x() - windowSize.width() / 2,
        availableGeometry.center().y() - windowSize.height() / 2);
}

QPoint boundedTopLeft(const QRect &availableGeometry, const QSize &windowSize, const QPoint &requestedTopLeft) {
    if (!availableGeometry.isValid()) {
        return requestedTopLeft;
    }

    const int minX = availableGeometry.left();
    const int minY = availableGeometry.top();
    const int maxX = qMax(minX, availableGeometry.right() - windowSize.width() + 1);
    const int maxY = qMax(minY, availableGeometry.bottom() - windowSize.height() + 1);

    return QPoint(
        qBound(minX, requestedTopLeft.x(), maxX),
        qBound(minY, requestedTopLeft.y(), maxY));
}

bool scrollListWidgetFromWheel(QListWidget *listWidget, QWheelEvent *wheelEvent) {
    if (listWidget == nullptr || wheelEvent == nullptr) {
        return false;
    }

    QScrollBar *scrollBar = listWidget->verticalScrollBar();
    if (scrollBar == nullptr || scrollBar->maximum() <= 0) {
        return false;
    }

    int delta = 0;
    if (!wheelEvent->pixelDelta().isNull()) {
        delta = wheelEvent->pixelDelta().y();
    } else if (!wheelEvent->angleDelta().isNull()) {
        delta = (wheelEvent->angleDelta().y() / 120) * qMax(24, scrollBar->singleStep() * 3);
    }

    if (delta == 0) {
        return true;
    }

    scrollBar->setValue(scrollBar->value() - delta);
    return true;
}

QString scaleStyleSheetPixels(const QString &styleSheet, double scale) {
    if (scale >= 0.999) {
        return styleSheet;
    }

    QString scaled = styleSheet;
    QRegularExpression pxPattern(R"((\d+(?:\.\d+)?)px)");
    QRegularExpressionMatchIterator it = pxPattern.globalMatch(styleSheet);
    int offset = 0;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString captured = match.captured(1);
        bool ok = false;
        const double value = captured.toDouble(&ok);
        if (!ok) {
            continue;
        }

        const int scaledValue = value <= 0.0 ? 0 : qMax(1, qRound(value * scale));
        const QString replacement = QString::number(scaledValue) + "px";
        scaled.replace(match.capturedStart(0) + offset, match.capturedLength(0), replacement);
        offset += replacement.size() - match.capturedLength(0);
    }

    return scaled;
}

QString appConfigDirPath() {
    const QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!configPath.isEmpty()) {
        return configPath;
    }

    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataPath.isEmpty()) {
        return dataPath;
    }

    const QString appDirPath = QCoreApplication::applicationDirPath();
    if (!appDirPath.isEmpty()) {
        return appDirPath;
    }

    return QDir::currentPath();
}

void installWheelBlocker(QWidget *widget, bool includeChildren = false) {
    if (widget == nullptr) {
        return;
    }

    auto *blocker = new WheelBlockerFilter(widget);
    widget->installEventFilter(blocker);

    if (!includeChildren) {
        return;
    }

    const auto children = widget->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->installEventFilter(blocker);
    }
}

void installEventFilterOnWidgetTree(QObject *filter, QWidget *widget) {
    if (filter == nullptr || widget == nullptr) {
        return;
    }

    widget->installEventFilter(filter);
    const auto children = widget->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->installEventFilter(filter);
    }
}

bool ensureParentDir(const QString &filePath) {
    const QFileInfo fileInfo(filePath);
    QDir dir(fileInfo.absolutePath());
    return dir.exists() || dir.mkpath(".");
}

QLabel *ensureTrailingIcon(QWidget *field, const QString &objectName) {
    if (field == nullptr) {
        return nullptr;
    }

    if (QLabel *icon = field->findChild<QLabel *>(objectName); icon != nullptr) {
        return icon;
    }

    auto *icon = new QLabel(field);
    icon->setObjectName(objectName);
    icon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    icon->setPixmap(QPixmap(":/icons/chevron-down.svg"));
    icon->setScaledContents(false);
    icon->show();
    return icon;
}

QDate todoDisplayEndDate(const TodoItem &todo) {
    if (!todo.date.isValid()) {
        return {};
    }

    if (todo.dueAt.isValid()) {
        const QDate dueDate = todo.dueAt.date();
        if (dueDate.isValid() && dueDate > todo.date) {
            return dueDate;
        }
    }

    return todo.date;
}

bool todoCoversDate(const TodoItem &todo, const QDate &date) {
    if (!date.isValid() || !todo.date.isValid()) {
        return false;
    }

    return date >= todo.date && date <= todoDisplayEndDate(todo);
}

void setElidedLabelText(QLabel *label, const QString &text, int maxWidth) {
    if (label == nullptr) {
        return;
    }
    const QFontMetrics metrics(label->font());
    label->setText(metrics.elidedText(text, Qt::ElideRight, maxWidth));
    label->setToolTip(text);
}

void setElidedButtonText(QPushButton *button, const QString &text, int maxWidth) {
    if (button == nullptr) {
        return;
    }
    const QFontMetrics metrics(button->font());
    button->setText(metrics.elidedText(text, Qt::ElideRight, maxWidth));
    button->setToolTip(text);
}

bool showAppDialog(QWidget *parent,
                   AppDialogKind kind,
                   const QString &title,
                   const QString &message,
                   const QString &confirmText = QStringLiteral("确定"),
                   const QString &cancelText = QStringLiteral("取消")) {
    QDialog dialog(parent);
    dialog.setObjectName("appDialog");
    dialog.setWindowFlag(Qt::FramelessWindowHint, true);
    dialog.setWindowFlag(Qt::Dialog, true);
    dialog.setModal(true);
    dialog.setSizeGripEnabled(false);
    dialog.setAttribute(Qt::WA_TranslucentBackground, true);
    dialog.setStyleSheet(R"(
        QDialog#appDialog {
            background: transparent;
        }
        QFrame#appDialogPanel {
            background: #fafafa;
            border: 1px solid #e8e8e8;
            border-radius: 20px;
        }
        QFrame#appDialogPanel QLabel {
            background: transparent;
            color: #2f3437;
        }
        QLabel#appDialogHeaderTitle {
            font-size: 16px;
            font-weight: 700;
            color: #191919;
        }
        QLabel#appDialogTitle {
            font-size: 18px;
            font-weight: 700;
            color: #191919;
        }
        QLabel#appDialogMessage {
            font-size: 14px;
            color: #4d5560;
        }
        QLabel#appDialogIcon {
            min-width: 44px;
            max-width: 44px;
            min-height: 44px;
            max-height: 44px;
            border-radius: 22px;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#appDialogIcon[kind="info"] {
            background: #eaf4ff;
            color: #2f80ed;
        }
        QLabel#appDialogIcon[kind="warning"] {
            background: #fff2e8;
            color: #f08a24;
        }
        QLabel#appDialogIcon[kind="confirm"] {
            background: #eaf8f0;
            color: #07c160;
        }
        QPushButton {
            background: #f2f2f2;
            border: 1px solid #e5e5e5;
            border-radius: 14px;
            padding: 10px 18px;
            color: #191919;
            font-weight: 600;
            min-width: 88px;
        }
        QPushButton:hover {
            background: #ebebeb;
        }
        QPushButton#primaryButton {
            background: #07c160;
            color: #ffffff;
            border: 1px solid #07c160;
        }
        QPushButton#primaryButton:hover {
            background: #06ad56;
            border: 1px solid #06ad56;
        }
        QPushButton#secondaryButton {
            background: #f2f2f2;
            color: #191919;
            border: 1px solid #e5e5e5;
        }
        QPushButton#secondaryButton:hover {
            background: #ebebeb;
        }
        QPushButton#dialogCloseButton {
            background: #f3f3f3;
            border: 1px solid #e2e2e2;
            border-radius: 12px;
            min-width: 28px;
            max-width: 28px;
            min-height: 28px;
            max-height: 28px;
            padding: 0;
            color: #6f7782;
            font-size: 16px;
            font-weight: 700;
        }
        QPushButton#dialogCloseButton:hover {
            background: #e8e8e8;
            border: 1px solid #d5d5d5;
        }
    )");

    auto *rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(0);

    auto *panel = new QFrame(&dialog);
    panel->setObjectName("appDialogPanel");
    rootLayout->addWidget(panel);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(14);

    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);
    headerRow->addSpacing(28);
    auto *headerTitle = new QLabel(title, panel);
    headerTitle->setObjectName("appDialogHeaderTitle");
    headerTitle->setAlignment(Qt::AlignCenter);
    auto *closeButton = new QPushButton(QStringLiteral("×"), panel);
    closeButton->setObjectName("dialogCloseButton");
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    headerRow->addWidget(headerTitle, 1);
    headerRow->addWidget(closeButton, 0, Qt::AlignRight);
    layout->addLayout(headerRow);

    auto *contentRow = new QHBoxLayout();
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(14);
    contentRow->setAlignment(Qt::AlignTop);

    auto *iconFrame = new QLabel(panel);
    iconFrame->setObjectName("appDialogIcon");
    iconFrame->setAlignment(Qt::AlignCenter);
    switch (kind) {
    case AppDialogKind::Info:
        iconFrame->setProperty("kind", "info");
        iconFrame->setText("i");
        break;
    case AppDialogKind::Warning:
        iconFrame->setProperty("kind", "warning");
        iconFrame->setText("!");
        break;
    case AppDialogKind::Confirm:
    default:
        iconFrame->setProperty("kind", "confirm");
        iconFrame->setText("?");
        break;
    }

    auto *textWrap = new QVBoxLayout();
    textWrap->setContentsMargins(0, 0, 0, 0);
    textWrap->setSpacing(6);

    auto *titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName("appDialogTitle");
    titleLabel->setWordWrap(true);
    auto *messageLabel = new QLabel(message, panel);
    messageLabel->setObjectName("appDialogMessage");
    messageLabel->setWordWrap(true);

    textWrap->addWidget(titleLabel);
    textWrap->addWidget(messageLabel);
    contentRow->addWidget(iconFrame, 0, Qt::AlignTop);
    contentRow->addLayout(textWrap, 1);
    layout->addLayout(contentRow);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(10);
    buttonRow->addStretch(1);

    bool accepted = false;
    if (kind == AppDialogKind::Confirm) {
        auto *cancelButton = new QPushButton(cancelText, panel);
        cancelButton->setObjectName("secondaryButton");
        auto *confirmButton = new QPushButton(confirmText, panel);
        confirmButton->setObjectName("primaryButton");
        QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
        QObject::connect(confirmButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        buttonRow->addWidget(cancelButton);
        buttonRow->addWidget(confirmButton);
    } else {
        auto *confirmButton = new QPushButton(confirmText, panel);
        confirmButton->setObjectName("primaryButton");
        QObject::connect(confirmButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        buttonRow->addWidget(confirmButton);
    }

    layout->addLayout(buttonRow);
    panel->setFixedWidth(420);
    if (kind == AppDialogKind::Confirm) {
        accepted = dialog.exec() == QDialog::Accepted;
    } else {
        dialog.exec();
        accepted = true;
    }
    return accepted;
}

void showAppWarningDialog(QWidget *parent, const QString &title, const QString &message) {
    showAppDialog(parent, AppDialogKind::Warning, title, message);
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setObjectName("HTodoWindow");
    setWindowTitle("HTodo");
    setWindowIcon(createAppIcon());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("mainTabs");
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildTodoTab(), "任务");
    m_tabs->addTab(buildPomodoroTab(), "番茄钟");
    m_tabs->addTab(buildStatsTab(), "统计");

    setCentralWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        applyWindowPresetForTab(index);
        saveUiState();
    });

    connect(m_addTodoButton, &QPushButton::clicked, this, &MainWindow::addTodo);
    connect(m_todoInput, &QLineEdit::returnPressed, this, &MainWindow::addTodo);
    connect(m_cancelEditButton, &QPushButton::clicked, this, &MainWindow::cancelTodoEdit);
    connect(m_todoList, &QListWidget::itemSelectionChanged, this, &MainWindow::onTodoSelectionChanged);
    connect(m_dueAtEnabled, &QCheckBox::toggled, m_dueAtInput, &QDateTimeEdit::setEnabled);
    connect(m_dailyPlanEnabled, &QCheckBox::toggled, m_planEndDateInput, &QDateEdit::setEnabled);
    connect(m_dailyPlanEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled && m_planEndDateInput != nullptr && m_taskDateInput != nullptr
            && m_planEndDateInput->date() < m_taskDateInput->date()) {
            m_planEndDateInput->setDate(m_taskDateInput->date());
        }
    });
    connect(m_dateNavigator, &QCalendarWidget::selectionChanged, this, [this]() {
        const QDate selected = m_dateNavigator->selectedDate();
        if (!selected.isValid() || selected == m_selectedDate) {
            return;
        }

        m_selectedDate = selected;
        if (m_editingTodoId.isEmpty()) {
            m_taskDateInput->setDate(m_selectedDate);
        }
        refreshTodoList();
        saveUiState();
        if (m_todoStandardHeaderPanel != nullptr) {
            m_todoStandardHeaderPanel->hide();
        }
    });
    connect(m_taskDateInput, &QDateEdit::dateChanged, this, [this](const QDate &date) {
        if (!m_dueAtEnabled->isChecked()) {
            return;
        }

        QDateTime dueAt = m_dueAtInput->dateTime();
        dueAt.setDate(date);
        m_dueAtInput->setDateTime(dueAt);
    });
    connect(m_taskDateInput, &QDateEdit::dateChanged, this, [this](const QDate &date) {
        if (!m_dailyPlanEnabled->isChecked()) {
            return;
        }
        if (m_planEndDateInput->date() < date) {
            m_planEndDateInput->setDate(date);
        }
    });
    connect(m_viewModeFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshTodoList();
        saveUiState();
    });
    connect(m_priorityFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshTodoList();
        saveUiState();
    });
    connect(m_tagFilterInput, &TagSelectorWidget::tagsChanged, this, [this]() {
        refreshTodoList();
        saveUiState();
    });
    connect(m_prevDayButton, &QPushButton::clicked, this, [this]() {
        if (m_dateNavigator != nullptr) {
            m_dateNavigator->setSelectedDate(m_selectedDate.addDays(-1));
        }
    });
    connect(m_todayQuickButton, &QPushButton::clicked, this, [this]() {
        if (m_dateNavigator != nullptr) {
            m_dateNavigator->setSelectedDate(QDate::currentDate());
        }
        if (m_todoStandardHeaderPanel != nullptr) {
            m_todoStandardHeaderPanel->hide();
        }
    });
    connect(m_nextDayButton, &QPushButton::clicked, this, [this]() {
        if (m_dateNavigator != nullptr) {
            m_dateNavigator->setSelectedDate(m_selectedDate.addDays(1));
        }
    });
    connect(m_todayLabel, &QPushButton::clicked, this, [this]() {
        if (m_todoStandardHeaderPanel == nullptr || m_todayLabel == nullptr) {
            return;
        }
        if (m_todoStandardHeaderPanel->isVisible()) {
            m_todoStandardHeaderPanel->hide();
            return;
        }
        m_todoStandardHeaderPanel->adjustSize();
        const QPoint popupPos = m_todayLabel->mapToGlobal(QPoint(0, m_todayLabel->height() + 8));
        m_todoStandardHeaderPanel->move(popupPos);
        m_todoStandardHeaderPanel->show();
        m_todoStandardHeaderPanel->raise();
    });
    connect(m_clearFilterButton, &QPushButton::clicked, this, [this]() {
        m_viewModeFilter->setCurrentIndex(0);
        m_priorityFilter->setCurrentIndex(0);
        if (m_tagFilterInput != nullptr) {
            m_tagFilterInput->setSelectedTags({});
        }
    });

    connect(m_startPauseButton, &QPushButton::clicked, this, &MainWindow::togglePomodoro);
    connect(m_stopPomodoroButton, &QPushButton::clicked, this, [this]() {
        if (m_storage.hasActiveTodoTiming()) {
            statusBar()->showMessage("任务手动计时进行中，当前时钟由任务计时占用。", 4000);
            return;
        }

        const QString taskId = m_pomodoroTaskSelector->currentData().toString();
        const qint64 elapsedSeconds = static_cast<qint64>(qMax(
            0, m_focusMinutes->value() * 60 - m_timer.remainingSeconds()));
        const bool shouldAccumulate = m_pomodoroTaskSelectionLocked
                                      && m_timer.phase() == PomodoroTimer::Phase::Focus
                                      && elapsedSeconds > 0
                                      && !taskId.isEmpty();

        if (shouldAccumulate) {
            if (m_storage.addTrackedSeconds(taskId, elapsedSeconds)) {
                statusBar()->showMessage(QString("已停止番茄钟，并累计 %1 到当前任务").arg(formatDuration(elapsedSeconds)), 3500);
            } else {
                statusBar()->showMessage("已停止番茄钟，但写入任务时长失败", 4000);
            }
        } else {
            statusBar()->showMessage("已停止番茄钟", 2500);
        }

        m_timer.reset();
        m_pomodoroTaskSelectionLocked = false;
        refreshStats();
        refreshPomodoroBindings();
        refreshPomodoroView(m_timer.remainingSeconds(), m_timer.phase());
        refreshPomodoroTaskTimingPanel();
    });
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::resetPomodoro);
    connect(m_pomodoroFocusCardButton, &QPushButton::clicked, this, [this]() {
        togglePomodoroFocusCard();
    });
    connect(m_pomodoroTaskSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshPomodoroTaskTimingPanel();
    });
    connect(m_pomodoroTaskTimingButton, &QPushButton::clicked, this, [this]() {
        if (m_pomodoroTaskSelector == nullptr) {
            return;
        }
        toggleTodoTiming(m_pomodoroTaskSelector->currentData().toString());
    });

    m_taskTimingRefreshTimer = new QTimer(this);
    m_taskTimingRefreshTimer->setInterval(1000);
    connect(m_taskTimingRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshTaskTimingUi);

    connect(m_focusMinutes, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        m_timer.setDurations(m_focusMinutes->value(), m_breakMinutes->value());
        saveUiState();
    });
    connect(m_breakMinutes, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        m_timer.setDurations(m_focusMinutes->value(), m_breakMinutes->value());
        saveUiState();
    });
    connect(m_presetBalancedButton, &QPushButton::clicked, this, [this]() {
        m_focusMinutes->setValue(25);
        m_breakMinutes->setValue(5);
    });
    connect(m_presetDeepFocusButton, &QPushButton::clicked, this, [this]() {
        m_focusMinutes->setValue(50);
        m_breakMinutes->setValue(10);
    });
    connect(m_presetQuickButton, &QPushButton::clicked, this, [this]() {
        m_focusMinutes->setValue(15);
        m_breakMinutes->setValue(3);
    });

    connect(&m_timer, &PomodoroTimer::tick, this, &MainWindow::onPomodoroTick);
    connect(&m_timer, &PomodoroTimer::phaseCompleted, this, &MainWindow::onPomodoroPhaseCompleted);
    connect(&m_timer, &PomodoroTimer::stateChanged, this, [this](bool running, PomodoroTimer::Phase) {
        m_startPauseButton->setText(running ? "暂停" : "开始");
        refreshPomodoroTaskTimingPanel();
        refreshPomodoroView(m_timer.remainingSeconds(), m_timer.phase());
    });

    m_timer.setDurations(m_focusMinutes->value(), m_breakMinutes->value());
    m_timer.reset();
    updatePomodoroFocusCardButton();
    statusBar()->setSizeGripEnabled(false);
    setupTrayIcon();

    resetTodoForm();
    applyTheme();
    applyWindowLayoutMode();
    loadUiState();
    refreshAll();
    updateTrayActions();
}

void MainWindow::attachSingleInstanceServer(QLocalServer *server) {
    m_singleInstanceServer = server;
    if (m_singleInstanceServer == nullptr) {
        return;
    }

    connect(m_singleInstanceServer, &QLocalServer::newConnection, this, [this]() {
        if (m_singleInstanceServer == nullptr) {
            return;
        }

        while (QLocalSocket *socket = m_singleInstanceServer->nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket]() {
                socket->readAll();
                activateFromSingleInstanceMessage();
                socket->disconnectFromServer();
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        }
    });
}

void MainWindow::activateFromSingleInstanceMessage() {
    if (m_pomodoroFocusCard != nullptr && m_pomodoroFocusCard->isVisible()) {
        hidePomodoroFocusCard(true);
    } else if (isHidden()) {
        showFromTray();
    } else {
        showNormal();
        raise();
        activateWindow();
        updateTrayActions();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_quitRequested && m_trayIcon != nullptr && m_trayIcon->isVisible()) {
        saveUiState();
        hide();
        updateTrayActions();
        if (!m_trayHintShown) {
            m_trayIcon->showMessage("HTodo", "窗口已隐藏到系统托盘，可通过托盘图标恢复。", QSystemTrayIcon::Information, 2500);
            m_trayHintShown = true;
        }
        event->ignore();
        return;
    }

    saveUiState();
    if (m_pomodoroFocusCard != nullptr) {
        m_pomodoroFocusCard->hide();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_todoList != nullptr && m_todoList->count() > 0) {
        refreshTodoList();
    }
    if (isVisible() && !isMinimized() && !isMaximized()) {
        saveUiState();
    }
}

void MainWindow::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event);
    if (isVisible() && !isMinimized() && !isMaximized()) {
        saveUiState();
    }
}

QString MainWindow::settingsFilePath() const {
    const QString appName = QCoreApplication::applicationName().isEmpty()
                                ? QStringLiteral("HTodo")
                                : QCoreApplication::applicationName();
    return QDir(appConfigDirPath()).filePath(appName + ".ini");
}

QString MainWindow::windowPositionSettingsKey(WindowLayoutMode mode) const {
    switch (mode) {
    case WindowLayoutMode::Compact:
        return QStringLiteral("window/position_compact");
    case WindowLayoutMode::Standard:
    default:
        return QStringLiteral("window/position_standard");
    }
}

QPoint MainWindow::savedWindowPosition(WindowLayoutMode mode) const {
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    const QPoint modeSpecificPosition = settings.value(windowPositionSettingsKey(mode)).toPoint();
    if (!modeSpecificPosition.isNull()) {
        return modeSpecificPosition;
    }
    return settings.value("window/position").toPoint();
}

QRect MainWindow::savedWindowGeometry() const {
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    const QRect frameGeometry = settings.value("window/frame_geometry").toRect();
    if (frameGeometry.isValid() && !frameGeometry.isNull()) {
        return frameGeometry;
    }

    const QRect geometry = settings.value("window/geometry").toRect();
    if (geometry.isValid() && !geometry.isNull()) {
        return geometry;
    }

    const QPoint topLeft = savedWindowPosition(m_windowLayoutMode);
    if (topLeft.isNull()) {
        return {};
    }
    return QRect(topLeft, size());
}

QScreen *MainWindow::screenByName(const QString &name) const {
    if (name.isEmpty()) {
        return nullptr;
    }
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *candidate : screens) {
        if (candidate != nullptr && candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

QScreen *MainWindow::screenForRectCenter(const QRect &rect) const {
    if (!rect.isValid()) {
        return nullptr;
    }

    const QPoint center = rect.center();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *candidate : screens) {
        if (candidate != nullptr && candidate->availableGeometry().contains(center)) {
            return candidate;
        }
    }
    return nullptr;
}

void MainWindow::setupTrayIcon() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    QIcon trayIcon = windowIcon();
    if (trayIcon.isNull()) {
        trayIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
        setWindowIcon(trayIcon);
    }

    m_trayMenu = new QMenu(this);
    m_showWindowAction = m_trayMenu->addAction("显示主窗口");
    m_hideWindowAction = m_trayMenu->addAction("隐藏主窗口");
    m_trayMenu->addSeparator();
    m_quitAction = m_trayMenu->addAction("退出");

    m_trayIcon = new QSystemTrayIcon(trayIcon, this);
    m_trayIcon->setToolTip("HTodo");
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_showWindowAction, &QAction::triggered, this, &MainWindow::showFromTray);
    connect(m_hideWindowAction, &QAction::triggered, this, [this]() {
        saveUiState();
        hide();
        updateTrayActions();
    });
    connect(m_quitAction, &QAction::triggered, this, [this]() {
        m_quitRequested = true;
        saveUiState();
        if (m_singleInstanceServer != nullptr) {
            m_singleInstanceServer->close();
        }
        if (m_pomodoroFocusCard != nullptr) {
            m_pomodoroFocusCard->hide();
        }
        if (m_trayIcon != nullptr) {
            m_trayIcon->hide();
        }
        close();
        QCoreApplication::quit();
    });
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showFromTray();
        }
    });

    m_trayIcon->show();
}

void MainWindow::updateTrayActions() {
    if (m_showWindowAction != nullptr) {
        m_showWindowAction->setEnabled(isHidden());
    }
    if (m_hideWindowAction != nullptr) {
        m_hideWindowAction->setEnabled(!isHidden());
    }
}

void MainWindow::showFromTray() {
    show();
    raise();
    activateWindow();
    updateTrayActions();
}

int MainWindow::scaledMetric(int baseValue, int minimum) const {
    return qMax(minimum, qRound(baseValue * m_uiScale));
}

void MainWindow::applyScaledMetrics() {
    const int filterButtonHeight = scaledMetric(36, 28);
    const int fieldHeight = scaledMetric(38, 30);
    const int selectorHeight = scaledMetric(44, 34);
    const int compactCalendarHeight = scaledMetric(204, 150);
    const int standardCalendarHeight = scaledMetric(232, 170);
    const int statPillHeight = scaledMetric(52, 38);
    const int actionButtonWidth = scaledMetric(120, 90);
    const int pomodoroNumberWidth = scaledMetric(150, 110);
    const int pomodoroDialSize = scaledMetric(220, 150);
    const int pomodoroHistoryHeight = scaledMetric(120, 88);
    const int chartHeight = scaledMetric(220, 160);
    const int metricCardHeight = scaledMetric(132, 96);
    const int heroCardHeight = scaledMetric(118, 88);
    const int narrativeCardHeight = scaledMetric(128, 92);

    if (m_filterToggleButton != nullptr) {
        m_filterToggleButton->setMinimumHeight(filterButtonHeight);
    }
    if (m_prevDayButton != nullptr) {
        m_prevDayButton->setMinimumHeight(filterButtonHeight);
    }
    if (m_todayQuickButton != nullptr) {
        m_todayQuickButton->setMinimumHeight(filterButtonHeight);
    }
    if (m_nextDayButton != nullptr) {
        m_nextDayButton->setMinimumHeight(filterButtonHeight);
    }
    if (m_dateNavigator != nullptr) {
        m_dateNavigator->setFixedHeight(m_windowLayoutMode == WindowLayoutMode::Compact
                                            ? compactCalendarHeight
                                            : standardCalendarHeight);
    }

    if (m_dayTaskCountLabel != nullptr) {
        m_dayTaskCountLabel->setMinimumHeight(statPillHeight);
    }
    if (m_dayDoneCountLabel != nullptr) {
        m_dayDoneCountLabel->setMinimumHeight(statPillHeight);
    }
    if (m_dayFocusCountLabel != nullptr) {
        m_dayFocusCountLabel->setMinimumHeight(statPillHeight);
    }

    if (m_todoInput != nullptr) {
        m_todoInput->setMinimumHeight(fieldHeight);
    }
    if (m_taskDateInput != nullptr) {
        m_taskDateInput->setMinimumHeight(fieldHeight);
    }
    if (m_planEndDateInput != nullptr) {
        m_planEndDateInput->setMinimumHeight(fieldHeight);
    }
    if (m_priorityInput != nullptr) {
        m_priorityInput->setMinimumHeight(fieldHeight);
    }
    if (m_dueAtInput != nullptr) {
        m_dueAtInput->setMinimumHeight(fieldHeight);
    }
    if (m_addTodoButton != nullptr) {
        m_addTodoButton->setMinimumHeight(fieldHeight);
        m_addTodoButton->setMinimumWidth(actionButtonWidth);
    }
    if (m_cancelEditButton != nullptr) {
        m_cancelEditButton->setMinimumHeight(fieldHeight);
        m_cancelEditButton->setMinimumWidth(actionButtonWidth);
    }

    if (m_pomodoroTaskSelector != nullptr) {
        m_pomodoroTaskSelector->setMinimumHeight(selectorHeight);
    }
    if (m_focusMinutes != nullptr) {
        m_focusMinutes->setMinimumHeight(selectorHeight);
        m_focusMinutes->setMinimumWidth(pomodoroNumberWidth);
    }
    if (m_breakMinutes != nullptr) {
        m_breakMinutes->setMinimumHeight(selectorHeight);
        m_breakMinutes->setMinimumWidth(pomodoroNumberWidth);
    }
    if (m_pomodoroTaskTimingButton != nullptr) {
        m_pomodoroTaskTimingButton->setMinimumHeight(scaledMetric(40, 30));
    }
    if (m_pomodoroDial != nullptr) {
        m_pomodoroDial->setFixedSize(pomodoroDialSize, pomodoroDialSize);
    }
    if (m_startPauseButton != nullptr) {
        m_startPauseButton->setMinimumHeight(fieldHeight);
    }
    if (m_stopPomodoroButton != nullptr) {
        m_stopPomodoroButton->setMinimumHeight(fieldHeight);
    }
    if (m_resetButton != nullptr) {
        m_resetButton->setMinimumHeight(fieldHeight);
    }
    if (m_pomodoroFocusCardButton != nullptr) {
        m_pomodoroFocusCardButton->setMinimumHeight(fieldHeight);
    }
    if (m_pomodoroHistoryCard != nullptr) {
        m_pomodoroHistoryCard->setMinimumHeight(pomodoroHistoryHeight);
    }

    if (m_tagTimingChart != nullptr) {
        m_tagTimingChart->setMinimumHeight(chartHeight);
    }
    if (m_tagFocusChart != nullptr) {
        m_tagFocusChart->setMinimumHeight(chartHeight);
    }
    if (m_totalTodayCard != nullptr) {
        m_totalTodayCard->setMinimumHeight(metricCardHeight);
    }
    if (m_completedTodayCard != nullptr) {
        m_completedTodayCard->setMinimumHeight(metricCardHeight);
    }
    if (m_completionRateCard != nullptr) {
        m_completionRateCard->setMinimumHeight(metricCardHeight);
    }
    if (m_focusTodayCard != nullptr) {
        m_focusTodayCard->setMinimumHeight(metricCardHeight);
    }
    if (m_focusWeekCard != nullptr) {
        m_focusWeekCard->setMinimumHeight(metricCardHeight);
    }
    if (m_taskTimingTodayCard != nullptr) {
        m_taskTimingTodayCard->setMinimumHeight(metricCardHeight);
    }
    if (m_activeTaskTimingCard != nullptr) {
        m_activeTaskTimingCard->setMinimumHeight(metricCardHeight);
    }
    if (m_statsHeroLabel != nullptr) {
        m_statsHeroLabel->setMinimumHeight(heroCardHeight);
    }
    if (m_statsFocusInsightLabel != nullptr) {
        m_statsFocusInsightLabel->setMinimumHeight(narrativeCardHeight);
    }
    if (m_statsTimingInsightLabel != nullptr) {
        m_statsTimingInsightLabel->setMinimumHeight(narrativeCardHeight);
    }

    QFont baseFont = font();
    baseFont.setPointSizeF(qBound(9.0, 10.5 * m_uiScale, 10.5));
    setFont(baseFont);
}

void MainWindow::loadUiState() {
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    const QRect savedGeometry = savedWindowGeometry();
    const QString savedScreenName = settings.value("window/screen_name").toString();
    QScreen *targetScreen = screenByName(savedScreenName);
    if (targetScreen == nullptr) {
        targetScreen = screenForRectCenter(savedGeometry);
    }
    if (targetScreen == nullptr) {
        targetScreen = screen();
    }
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }

    const QRect targetAvailableGeometry = availableGeometryForScreen(targetScreen);
    QSize targetSize = fixedWindowSizeForGeometry(targetAvailableGeometry);
    m_uiScale = static_cast<double>(targetSize.width()) / static_cast<double>(kBaseWindowWidth);
    applyTheme();
    m_windowLayoutMode = WindowLayoutMode::Compact;
    applyWindowLayoutMode();
    applyScaledMetrics();
    ensurePolished();
    if (QLayout *centralLayout = centralWidget() != nullptr ? centralWidget()->layout() : nullptr; centralLayout != nullptr) {
        centralLayout->activate();
    }
    targetSize = boundedWindowSize(targetAvailableGeometry, targetSize, minimumSizeHint().expandedTo(sizeHint()));
    m_uiScale = static_cast<double>(targetSize.width()) / static_cast<double>(kBaseWindowWidth);
    applyTheme();
    applyWindowLayoutMode();
    applyScaledMetrics();
    setFixedSize(targetSize);

    m_selectedDate = QDate::currentDate();
    if (m_dateNavigator != nullptr) {
        const QSignalBlocker blocker(m_dateNavigator);
        m_dateNavigator->setSelectedDate(m_selectedDate);
    }
    if (m_taskDateInput != nullptr && m_editingTodoId.isEmpty()) {
        const QSignalBlocker blocker(m_taskDateInput);
        m_taskDateInput->setDate(m_selectedDate);
    }

    if (m_viewModeFilter != nullptr) {
        const int savedMode = settings.value("todo/view_mode", static_cast<int>(TodoListMode::Today)).toInt();
        const QSignalBlocker blocker(m_viewModeFilter);
        for (int i = 0; i < m_viewModeFilter->count(); ++i) {
            if (m_viewModeFilter->itemData(i).toInt() == savedMode) {
                m_viewModeFilter->setCurrentIndex(i);
                break;
            }
        }
    }

    if (m_priorityFilter != nullptr) {
        const int savedPriority = settings.value("todo/priority_filter", -1).toInt();
        const QSignalBlocker blocker(m_priorityFilter);
        for (int i = 0; i < m_priorityFilter->count(); ++i) {
            if (m_priorityFilter->itemData(i).toInt() == savedPriority) {
                m_priorityFilter->setCurrentIndex(i);
                break;
            }
        }
    }

    if (m_tagFilterInput != nullptr) {
        const QSignalBlocker blocker(m_tagFilterInput);
        m_tagFilterInput->setAvailableTags(m_storage.availableTags());
        QStringList savedTags = settings.value("todo/tag_filter_tags").toStringList();
        if (savedTags.isEmpty()) {
            const QString legacyTagFilter = settings.value("todo/tag_filter").toString().trimmed();
            if (!legacyTagFilter.isEmpty()) {
                savedTags = {legacyTagFilter};
            }
        }
        m_tagFilterInput->setSelectedTags(savedTags);
    }
    if (m_focusMinutes != nullptr) {
        const int savedFocusMinutes = settings.value("pomodoro/focus_minutes", m_focusMinutes->value()).toInt();
        const QSignalBlocker blocker(m_focusMinutes);
        m_focusMinutes->setValue(qBound(m_focusMinutes->minimum(), savedFocusMinutes, m_focusMinutes->maximum()));
    }

    if (m_breakMinutes != nullptr) {
        const int savedBreakMinutes = settings.value("pomodoro/break_minutes", m_breakMinutes->value()).toInt();
        const QSignalBlocker blocker(m_breakMinutes);
        m_breakMinutes->setValue(qBound(m_breakMinutes->minimum(), savedBreakMinutes, m_breakMinutes->maximum()));
    }
    m_timer.setDurations(m_focusMinutes->value(), m_breakMinutes->value());

    if (m_tabs != nullptr) {
        const int savedTab = settings.value("window/current_tab", 0).toInt();
        const int boundedTab = qBound(0, savedTab, m_tabs->count() - 1);
        const QSignalBlocker blocker(m_tabs);
        m_tabs->setCurrentIndex(boundedTab);
    }
    applyWindowPresetForTab(m_tabs != nullptr ? m_tabs->currentIndex() : 0);

    QPoint requestedTopLeft;
    if (savedGeometry.isValid() && !savedGeometry.isNull()) {
        requestedTopLeft = savedGeometry.topLeft();
    } else {
        requestedTopLeft = settings.value("window/position").toPoint();
    }

    if (!requestedTopLeft.isNull()) {
        move(boundedTopLeft(targetAvailableGeometry, frameGeometry().size(), requestedTopLeft));
    } else {
        move(boundedTopLeft(targetAvailableGeometry, frameGeometry().size(),
                            centeredTopLeft(targetAvailableGeometry, frameGeometry().size())));
    }
    m_trayHintShown = settings.value("tray/hint_shown", false).toBool();
    refreshStoragePathUi();
}

void MainWindow::saveUiState() const {
    ensureParentDir(settingsFilePath());
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    const QRect normalRect = isMaximized() ? normalGeometry() : geometry();
    const QRect frameRect = frameGeometry();
    settings.setValue("window/position", frameRect.topLeft());
    settings.setValue("window/frame_geometry", frameRect);
    settings.setValue("window/geometry", normalRect);
    if (QScreen *currentScreen = screen(); currentScreen != nullptr) {
        settings.setValue("window/screen_name", currentScreen->name());
    } else {
        settings.remove("window/screen_name");
    }
    settings.remove("window/size");
    settings.setValue("window/current_tab", m_tabs != nullptr ? m_tabs->currentIndex() : 0);
    settings.setValue("todo/selected_date", m_selectedDate.toString(Qt::ISODate));
    settings.setValue("todo/view_mode", m_viewModeFilter != nullptr ? m_viewModeFilter->currentData().toInt() : 0);
    settings.setValue("todo/priority_filter", m_priorityFilter != nullptr ? m_priorityFilter->currentData().toInt() : -1);
    settings.setValue("todo/tag_filter_tags",
                      m_tagFilterInput != nullptr ? m_tagFilterInput->selectedTags() : QStringList());
    settings.remove("todo/tag_filter");
    settings.setValue("pomodoro/focus_minutes", m_focusMinutes != nullptr ? m_focusMinutes->value() : 25);
    settings.setValue("pomodoro/break_minutes", m_breakMinutes != nullptr ? m_breakMinutes->value() : 5);
    settings.setValue("tray/hint_shown", m_trayHintShown);
    settings.sync();
}

void MainWindow::applyWindowPresetForTab(int index) {
    Q_UNUSED(index);
}

void MainWindow::setWindowLayoutMode(WindowLayoutMode mode) {
    Q_UNUSED(mode);
    m_windowLayoutMode = WindowLayoutMode::Compact;
    applyWindowLayoutMode();
    applyWindowPresetForTab(m_tabs != nullptr ? m_tabs->currentIndex() : 0);
    resetTodoForm();
    refreshAll();
}

void MainWindow::applyWindowLayoutMode() {
    const bool compact = true;
    const int contentMargin = scaledMetric(compact ? 16 : 28, 10);
    const int contentSpacing = scaledMetric(compact ? 14 : 18, 8);
    const int workspaceSpacing = scaledMetric(compact ? 14 : 20, 8);
    const int rowSpacing = scaledMetric(compact ? 10 : 14, 6);

    if (m_todoContentLayout != nullptr) {
        m_todoContentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
        m_todoContentLayout->setSpacing(contentSpacing);
    }

    if (m_todoWorkspaceLayout != nullptr) {
        m_todoWorkspaceLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_todoWorkspaceLayout->setSpacing(workspaceSpacing);
    }

    if (m_todoMetaRow != nullptr) {
        m_todoMetaRow->setDirection(QBoxLayout::LeftToRight);
        m_todoMetaRow->setSpacing(rowSpacing);
    }

    if (m_todoBottomRow != nullptr) {
        m_todoBottomRow->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_todoBottomRow->setSpacing(rowSpacing);
    }

    if (m_todoSidebarCard != nullptr) {
        m_todoSidebarCard->setMinimumWidth(compact ? 0 : scaledMetric(344, 240));
        m_todoSidebarCard->setMaximumWidth(compact ? QWIDGETSIZE_MAX : scaledMetric(364, 260));
    }
    if (m_todoStandardPanel != nullptr) {
        m_todoStandardPanel->setVisible(true);
    }
    if (m_todoCompactDatePanel != nullptr) {
        m_todoCompactDatePanel->setVisible(true);
    }
    if (m_todoDateField != nullptr) {
        m_todoDateField->setVisible(true);
    }
    if (m_todoDueField != nullptr) {
        m_todoDueField->setVisible(true);
    }
    if (m_filterToggleButton != nullptr) {
        m_filterToggleButton->setVisible(true);
    }

    if (m_todoEditorCard != nullptr) {
        m_todoEditorCard->setMinimumHeight(compact ? 0 : scaledMetric(228, 160));
    }

    if (m_dateNavigator != nullptr) {
        m_dateNavigator->setFixedHeight(compact ? scaledMetric(204, 150) : scaledMetric(232, 170));
    }

    if (m_todoList != nullptr) {
        m_todoList->setMinimumHeight(0);
    }

    if (m_statsContentLayout != nullptr) {
        m_statsContentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
        m_statsContentLayout->setSpacing(scaledMetric(compact ? 14 : 16, 8));
    }
    if (m_pomodoroPresetPanel != nullptr) {
        m_pomodoroPresetPanel->setVisible(true);
    }
    if (m_pomodoroHistoryCard != nullptr) {
        m_pomodoroHistoryCard->setVisible(true);
    }
    if (m_statsStandardPanel != nullptr) {
        m_statsStandardPanel->setVisible(true);
    }
    if (m_statsRefreshButton != nullptr) {
        m_statsRefreshButton->setVisible(true);
    }

    rebuildStatsMetricLayout();
}

void MainWindow::rebuildStatsMetricLayout() {
    if (m_statsOverviewGrid == nullptr || m_statsDetailGrid == nullptr) {
        return;
    }

    while (QLayoutItem *item = m_statsOverviewGrid->takeAt(0)) {
        delete item;
    }
    while (QLayoutItem *item = m_statsDetailGrid->takeAt(0)) {
        delete item;
    }

    const bool compact = m_windowLayoutMode == WindowLayoutMode::Compact;
    if (m_activeTaskTimingCard != nullptr) {
        m_activeTaskTimingCard->setVisible(true);
    }
    m_statsOverviewGrid->setHorizontalSpacing(scaledMetric(compact ? 10 : 14, 6));
    m_statsOverviewGrid->setVerticalSpacing(scaledMetric(compact ? 10 : 14, 6));
    m_statsDetailGrid->setHorizontalSpacing(scaledMetric(compact ? 10 : 14, 6));
    m_statsDetailGrid->setVerticalSpacing(scaledMetric(compact ? 10 : 14, 6));

    if (compact) {
        m_statsOverviewGrid->addWidget(m_totalTodayCard, 0, 0);
        m_statsOverviewGrid->addWidget(m_completedTodayCard, 1, 0);
        m_statsOverviewGrid->addWidget(m_completionRateCard, 2, 0);
        m_statsOverviewGrid->addWidget(m_focusTodayCard, 3, 0);

        m_statsDetailGrid->addWidget(m_focusWeekCard, 0, 0);
        m_statsDetailGrid->addWidget(m_taskTimingTodayCard, 1, 0);
        m_statsDetailGrid->addWidget(m_activeTaskTimingCard, 2, 0);
        return;
    }

    m_statsOverviewGrid->addWidget(m_totalTodayCard, 0, 0);
    m_statsOverviewGrid->addWidget(m_completedTodayCard, 0, 1);
    m_statsOverviewGrid->addWidget(m_completionRateCard, 1, 0);
    m_statsOverviewGrid->addWidget(m_focusTodayCard, 1, 1);

    m_statsDetailGrid->addWidget(m_focusWeekCard, 0, 0);
    m_statsDetailGrid->addWidget(m_taskTimingTodayCard, 0, 1);
    m_statsDetailGrid->addWidget(m_activeTaskTimingCard, 1, 0, 1, 2);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (auto *button = qobject_cast<QPushButton *>(watched); button != nullptr) {
        const QString normalIconPath = button->property("headerNormalIcon").toString();
        const QString hoverIconPath = button->property("headerHoverIcon").toString();
        if (!normalIconPath.isEmpty() && !hoverIconPath.isEmpty()) {
            if (event->type() == QEvent::Enter) {
                button->setIcon(QIcon(hoverIconPath));
            } else if (event->type() == QEvent::Leave) {
                button->setIcon(QIcon(normalIconPath));
            }
        }

        const QString normalDeleteIconPath = button->property("normalIcon").toString();
        const QString hoverDeleteIconPath = button->property("hoverIcon").toString();
        if (!normalDeleteIconPath.isEmpty() && !hoverDeleteIconPath.isEmpty()) {
            if (event->type() == QEvent::Enter) {
                button->setIcon(QIcon(hoverDeleteIconPath));
            } else if (event->type() == QEvent::Leave) {
                button->setIcon(QIcon(normalDeleteIconPath));
            }
        }
    }

    if (event->type() == QEvent::Wheel) {
        QWidget *watchedWidget = qobject_cast<QWidget *>(watched);
        const bool onTodoList = m_todoList != nullptr
                                && (watched == m_todoList
                                    || watched == m_todoList->viewport()
                                    || (watchedWidget != nullptr && m_todoList->isAncestorOf(watchedWidget)));
        const bool onPlanList = m_planList != nullptr
                                && (watched == m_planList
                                    || watched == m_planList->viewport()
                                    || (watchedWidget != nullptr && m_planList->isAncestorOf(watchedWidget)));
        const bool onCalendar = m_dateNavigator != nullptr
                                && (watched == m_dateNavigator
                                    || (watchedWidget != nullptr && m_dateNavigator->isAncestorOf(watchedWidget)));
        const bool onTaskPopupCalendar = m_taskDatePopupCalendar != nullptr
                                         && (watched == m_taskDatePopupCalendar
                                             || (watchedWidget != nullptr
                                                 && m_taskDatePopupCalendar->isAncestorOf(watchedWidget)));
        const bool onDuePopupCalendar = m_dueAtPopupCalendar != nullptr
                                        && (watched == m_dueAtPopupCalendar
                                            || (watchedWidget != nullptr
                                                && m_dueAtPopupCalendar->isAncestorOf(watchedWidget)));
        const bool onPlanPopupCalendar = m_planEndDatePopupCalendar != nullptr
                                         && (watched == m_planEndDatePopupCalendar
                                             || (watchedWidget != nullptr
                                                 && m_planEndDatePopupCalendar->isAncestorOf(watchedWidget)));
        const bool onTaskDateInput = m_taskDateInput != nullptr
                                     && (watched == m_taskDateInput
                                         || (watchedWidget != nullptr && m_taskDateInput->isAncestorOf(watchedWidget)));
        const bool onDueAtInput = m_dueAtInput != nullptr
                                  && (watched == m_dueAtInput
                                      || (watchedWidget != nullptr && m_dueAtInput->isAncestorOf(watchedWidget)));
        const bool onPlanDateInput = m_planEndDateInput != nullptr
                                     && (watched == m_planEndDateInput
                                         || (watchedWidget != nullptr
                                             && m_planEndDateInput->isAncestorOf(watchedWidget)));

        if (onTaskPopupCalendar || onDuePopupCalendar || onPlanPopupCalendar) {
            return true;
        }

        if (onTodoList) {
            return scrollListWidgetFromWheel(m_todoList, static_cast<QWheelEvent *>(event));
        }

        if (onPlanList) {
            return scrollListWidgetFromWheel(m_planList, static_cast<QWheelEvent *>(event));
        }

        if (onCalendar || onTaskDateInput || onDueAtInput || onPlanDateInput) {
            if (m_todoScrollArea != nullptr) {
                auto *wheelEvent = static_cast<QWheelEvent *>(event);
                QScrollBar *scrollBar = m_todoScrollArea->verticalScrollBar();
                if (scrollBar != nullptr) {
                    int delta = 0;
                    if (!wheelEvent->pixelDelta().isNull()) {
                        delta = wheelEvent->pixelDelta().y();
                    } else if (!wheelEvent->angleDelta().isNull()) {
                        delta = (wheelEvent->angleDelta().y() / 120) * qMax(24, scrollBar->singleStep() * 3);
                    }

                    if (delta != 0) {
                        scrollBar->setValue(scrollBar->value() - delta);
                    }
                }
            }
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

QWidget *MainWindow::buildTodoTab() {
    auto *tab = new QWidget(this);
    auto configureComboBox = [](RoundedComboBox *combo, int minimumWidth) {
        combo->setMinimumWidth(minimumWidth);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    };

    auto *rootLayout = new QVBoxLayout(tab);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_todoScrollArea = scrollArea;

    auto *content = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(18);
    m_todoContentLayout = layout;

    auto *headerLayout = new QVBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);
    auto *headerTitleRow = new QHBoxLayout();
    headerTitleRow->setContentsMargins(0, 0, 0, 0);
    headerTitleRow->setSpacing(8);
    auto configureHeaderIconButton = [this](QPushButton *button, const QString &normalPath, const QString &hoverPath) {
        button->setIcon(QIcon(normalPath));
        button->setProperty("headerNormalIcon", normalPath);
        button->setProperty("headerHoverIcon", hoverPath);
        button->installEventFilter(this);
    };
    m_prevDayButton = new QPushButton(tab);
    m_prevDayButton->setObjectName("headerIconButton");
    m_prevDayButton->setFixedSize(42, 42);
    m_prevDayButton->setCursor(Qt::PointingHandCursor);
    configureHeaderIconButton(m_prevDayButton, ":/icons/nav-prev.png", ":/icons/nav-prev-hover.png");
    m_prevDayButton->setIconSize(QSize(42, 42));

    m_todayLabel = new QPushButton(tab);
    m_todayLabel->setObjectName("pageTitle");
    m_todayLabel->setFlat(true);
    m_todayLabel->setCursor(Qt::PointingHandCursor);
    m_todayLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_todayLabel->setMinimumWidth(0);

    m_nextDayButton = new QPushButton(tab);
    m_nextDayButton->setObjectName("headerIconButton");
    m_nextDayButton->setFixedSize(42, 42);
    m_nextDayButton->setCursor(Qt::PointingHandCursor);
    configureHeaderIconButton(m_nextDayButton, ":/icons/nav-next.png", ":/icons/nav-next-hover.png");
    m_nextDayButton->setIconSize(QSize(42, 42));

    m_filterToggleButton = new QPushButton(tab);
    m_filterToggleButton->setObjectName("headerIconButton");
    m_filterToggleButton->setMinimumHeight(36);
    m_filterToggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_filterToggleButton->setToolTip("视图与筛选");
    configureHeaderIconButton(m_filterToggleButton, ":/icons/nav-filter.png", ":/icons/nav-filter-hover.png");
    m_filterToggleButton->setIconSize(QSize(42, 42));
    m_filterToggleButton->setFixedSize(42, 42);

    auto *leftHeaderBox = new QWidget(content);
    leftHeaderBox->setFixedWidth(92);
    auto *leftHeaderLayout = new QHBoxLayout(leftHeaderBox);
    leftHeaderLayout->setContentsMargins(0, 0, 0, 0);
    leftHeaderLayout->setSpacing(8);
    leftHeaderLayout->addWidget(m_prevDayButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftHeaderLayout->addStretch(1);

    auto *rightHeaderBox = new QWidget(content);
    rightHeaderBox->setFixedWidth(92);
    auto *rightHeaderLayout = new QHBoxLayout(rightHeaderBox);
    rightHeaderLayout->setContentsMargins(0, 0, 0, 0);
    rightHeaderLayout->setSpacing(8);
    rightHeaderLayout->addWidget(m_nextDayButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    rightHeaderLayout->addWidget(m_filterToggleButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    headerTitleRow->addWidget(leftHeaderBox, 0, Qt::AlignVCenter);
    headerTitleRow->addWidget(m_todayLabel, 1, Qt::AlignCenter);
    headerTitleRow->addWidget(rightHeaderBox, 0, Qt::AlignVCenter);
    headerLayout->addLayout(headerTitleRow);

    m_overdueReminderLabel = new QLabel(tab);
    m_overdueReminderLabel->setObjectName("overdueBanner");
    m_overdueReminderLabel->setVisible(false);

    auto *workspaceLayout = new QHBoxLayout();
    workspaceLayout->setSpacing(20);
    m_todoWorkspaceLayout = workspaceLayout;

    auto *sidebarCard = new QFrame(tab, Qt::Popup | Qt::FramelessWindowHint);
    sidebarCard->setObjectName("sidebarCard");
    sidebarCard->setMinimumWidth(344);
    sidebarCard->setMaximumWidth(364);
    m_todoSidebarCard = sidebarCard;
    auto *sidebarLayout = new QVBoxLayout(sidebarCard);
    sidebarLayout->setContentsMargins(16, 16, 16, 16);
    sidebarLayout->setSpacing(10);

    m_selectedDateLabel = new QLabel(formatDateTitle(m_selectedDate), sidebarCard);
    m_selectedDateLabel->setObjectName("sidebarTitle");
    m_selectedDateMetaLabel = new QLabel("单日视图的任务导航", sidebarCard);
    m_selectedDateMetaLabel->setObjectName("mutedText");
    m_todayQuickButton = new QPushButton("今天", sidebarCard);
    m_todayQuickButton->setObjectName("secondaryButton");
    m_todayQuickButton->setMinimumHeight(34);
    m_todayQuickButton->setCursor(Qt::PointingHandCursor);

    auto *sidebarHeaderRow = new QHBoxLayout();
    sidebarHeaderRow->setContentsMargins(0, 0, 0, 0);
    sidebarHeaderRow->setSpacing(8);
    sidebarHeaderRow->addWidget(m_selectedDateLabel, 1, Qt::AlignLeft | Qt::AlignVCenter);
    sidebarHeaderRow->addWidget(m_todayQuickButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_dateNavigator = new QCalendarWidget(sidebarCard);
    m_dateNavigator->setSelectedDate(m_selectedDate);
    m_dateNavigator->setGridVisible(false);
    m_dateNavigator->setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);
    m_dateNavigator->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_dateNavigator->setNavigationBarVisible(true);
    m_dateNavigator->setFixedHeight(232);
    m_dateNavigator->installEventFilter(this);
    const auto calendarChildren = m_dateNavigator->findChildren<QWidget *>();
    for (QWidget *child : calendarChildren) {
        child->installEventFilter(this);
    }

    m_dayTaskCountLabel = new QLabel(sidebarCard);
    m_dayTaskCountLabel->setObjectName("sidebarStatCard");
    m_dayTaskCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_dayDoneCountLabel = new QLabel(sidebarCard);
    m_dayDoneCountLabel->setObjectName("sidebarStatCard");
    m_dayDoneCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_dayFocusCountLabel = new QLabel(sidebarCard);
    m_dayFocusCountLabel->setObjectName("sidebarStatCard");
    m_dayFocusCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *statsLayout = new QGridLayout();
    statsLayout->setHorizontalSpacing(10);
    statsLayout->setVerticalSpacing(10);
    statsLayout->addWidget(m_dayTaskCountLabel, 0, 0);
    statsLayout->addWidget(m_dayDoneCountLabel, 0, 1);
    statsLayout->addWidget(m_dayFocusCountLabel, 1, 0, 1, 2);

    sidebarLayout->addLayout(sidebarHeaderRow);
    sidebarLayout->addWidget(m_selectedDateMetaLabel);
    sidebarLayout->addWidget(m_dateNavigator);
    sidebarLayout->addLayout(statsLayout);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(12);

    auto *editorCard = new QFrame(content);
    editorCard->setObjectName("surfaceCard");
    editorCard->setMinimumHeight(228);
    editorCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_todoEditorCard = editorCard;
    auto *editorCardLayout = new QVBoxLayout(editorCard);
    editorCardLayout->setContentsMargins(16, 16, 16, 16);
    editorCardLayout->setSpacing(12);

    m_editorCaptionLabel = new QLabel("新建任务", editorCard);
    m_editorCaptionLabel->setObjectName("sectionTitle");

    m_todoInput = new QLineEdit(tab);
    m_todoInput->setPlaceholderText("输入今天要做的事情...");
    m_todoInput->setMinimumHeight(38);

    m_taskDateInput = new QDateEdit(QDate::currentDate(), tab);
    m_taskDateInput->setDisplayFormat("yyyy-MM-dd");
    m_taskDateInput->setCalendarPopup(true);
    m_taskDateInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_taskDateInput->setMinimumHeight(38);
    m_taskDateInput->installEventFilter(this);
    for (QWidget *child : m_taskDateInput->findChildren<QWidget *>()) {
        child->installEventFilter(this);
    }
    m_taskDatePopupCalendar = m_taskDateInput->calendarWidget();
    if (m_taskDatePopupCalendar != nullptr) {
        m_taskDatePopupCalendar->installEventFilter(this);
        for (QWidget *child : m_taskDatePopupCalendar->findChildren<QWidget *>()) {
            child->installEventFilter(this);
        }
    }

    m_dailyPlanEnabled = new QCheckBox("每天重复", tab);
    m_planEndDateInput = new QDateEdit(QDate::currentDate(), tab);
    m_planEndDateInput->setDisplayFormat("yyyy-MM-dd");
    m_planEndDateInput->setCalendarPopup(true);
    m_planEndDateInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_planEndDateInput->setMinimumHeight(38);
    m_planEndDateInput->setEnabled(false);
    m_planEndDateInput->installEventFilter(this);
    for (QWidget *child : m_planEndDateInput->findChildren<QWidget *>()) {
        child->installEventFilter(this);
    }
    m_planEndDatePopupCalendar = m_planEndDateInput->calendarWidget();
    if (m_planEndDatePopupCalendar != nullptr) {
        m_planEndDatePopupCalendar->installEventFilter(this);
        for (QWidget *child : m_planEndDatePopupCalendar->findChildren<QWidget *>()) {
            child->installEventFilter(this);
        }
    }
    m_planHintLabel = new QLabel("保存后会在起止日期内每天生成 1 条任务。", tab);
    m_planHintLabel->setObjectName("mutedText");
    m_planHintLabel->setVisible(false);

    m_priorityInput = new RoundedComboBox(tab);
    m_priorityInput->addItem("高", static_cast<int>(TaskPriority::High));
    m_priorityInput->addItem("中", static_cast<int>(TaskPriority::Medium));
    m_priorityInput->addItem("低", static_cast<int>(TaskPriority::Low));
    m_priorityInput->setCurrentIndex(1);
    configureComboBox(m_priorityInput, 120);
    m_priorityInput->setMinimumHeight(38);

    m_dueAtEnabled = new QCheckBox("启用", tab);
    m_dueAtInput = new QDateTimeEdit(QDateTime::currentDateTime(), tab);
    m_dueAtInput->setDisplayFormat("yyyy-MM-dd HH:mm");
    m_dueAtInput->setCalendarPopup(true);
    m_dueAtInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_dueAtInput->setEnabled(false);
    m_dueAtInput->setMinimumHeight(38);
    installWheelBlocker(m_dueAtInput, true);
    m_dueAtInput->installEventFilter(this);
    for (QWidget *child : m_dueAtInput->findChildren<QWidget *>()) {
        child->installEventFilter(this);
    }
    m_dueAtPopupCalendar = m_dueAtInput->calendarWidget();
    if (m_dueAtPopupCalendar != nullptr) {
        m_dueAtPopupCalendar->installEventFilter(this);
        for (QWidget *child : m_dueAtPopupCalendar->findChildren<QWidget *>()) {
            child->installEventFilter(this);
        }
    }

    m_addTodoButton = new QPushButton("添加", tab);
    m_addTodoButton->setObjectName("primaryButton");
    m_addTodoButton->setMinimumHeight(38);
    m_addTodoButton->setMinimumWidth(120);
    m_cancelEditButton = new QPushButton("取消编辑", tab);
    m_cancelEditButton->setObjectName("secondaryButton");
    m_cancelEditButton->setMinimumHeight(38);
    m_cancelEditButton->setMinimumWidth(120);

    auto *editorLayout = new QVBoxLayout();
    editorLayout->setSpacing(10);

    auto *taskField = new QWidget(tab);
    auto *taskFieldLayout = new QVBoxLayout(taskField);
    taskFieldLayout->setContentsMargins(0, 0, 0, 0);
    taskFieldLayout->setSpacing(6);
    taskFieldLayout->addWidget(new QLabel("任务", taskField));
    taskFieldLayout->addWidget(m_todoInput);

    auto *standardPanel = new QWidget(editorCard);
    m_todoStandardPanel = standardPanel;
    auto *standardPanelLayout = new QVBoxLayout(standardPanel);
    standardPanelLayout->setContentsMargins(0, 0, 0, 0);
    standardPanelLayout->setSpacing(10);

    auto *metaRow = new QHBoxLayout();
    metaRow->setSpacing(8);
    m_todoMetaRow = metaRow;

    auto *dateField = new QWidget(tab);
    m_todoDateField = dateField;
    auto *dateFieldLayout = new QVBoxLayout(dateField);
    dateFieldLayout->setContentsMargins(0, 0, 0, 0);
    dateFieldLayout->setSpacing(4);
    dateFieldLayout->addWidget(new QLabel("开始日期", dateField));
    dateFieldLayout->addWidget(m_taskDateInput);

    auto *priorityField = new QWidget(tab);
    auto *priorityFieldLayout = new QVBoxLayout(priorityField);
    priorityFieldLayout->setContentsMargins(0, 0, 0, 0);
    priorityFieldLayout->setSpacing(4);
    priorityFieldLayout->addWidget(new QLabel("优先级", priorityField));
    priorityFieldLayout->addWidget(m_priorityInput);
    priorityField->setMaximumWidth(120);

    auto *dueField = new QWidget(tab);
    m_todoDueField = dueField;
    auto *dueFieldLayout = new QVBoxLayout(dueField);
    dueFieldLayout->setContentsMargins(0, 0, 0, 0);
    dueFieldLayout->setSpacing(4);
    auto *dueFieldTopRow = new QHBoxLayout();
    dueFieldTopRow->setContentsMargins(0, 0, 0, 0);
    dueFieldTopRow->setSpacing(6);
    dueFieldTopRow->addWidget(new QLabel("截止时间", dueField));
    dueFieldTopRow->addWidget(m_dueAtEnabled, 0, Qt::AlignLeft);
    dueFieldTopRow->addStretch(1);
    dueFieldLayout->addLayout(dueFieldTopRow);
    dueFieldLayout->addWidget(m_dueAtInput);

    auto *planField = new QWidget(tab);
    auto *planFieldLayout = new QVBoxLayout(planField);
    planFieldLayout->setContentsMargins(0, 0, 0, 0);
    planFieldLayout->setSpacing(4);
    planFieldLayout->addWidget(new QLabel("重复计划", planField));
    auto *planRow = new QHBoxLayout();
    planRow->setContentsMargins(0, 0, 0, 0);
    planRow->setSpacing(8);
    planRow->addWidget(m_dailyPlanEnabled);
    planRow->addWidget(m_planEndDateInput, 1);
    planFieldLayout->addLayout(planRow);
    planFieldLayout->addWidget(m_planHintLabel);
 
    metaRow->addWidget(dateField, 2);
    metaRow->addWidget(planField, 3);

    auto *metaSecondaryRow = new QHBoxLayout();
    metaSecondaryRow->setContentsMargins(0, 0, 0, 0);
    metaSecondaryRow->setSpacing(8);
    metaSecondaryRow->addWidget(dueField, 1);
    metaSecondaryRow->addWidget(priorityField, 0);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(10);
    m_todoBottomRow = bottomRow;

    auto *tagField = new QWidget(tab);
    auto *tagFieldLayout = new QVBoxLayout(tagField);
    tagFieldLayout->setContentsMargins(0, 0, 0, 0);
    tagFieldLayout->setSpacing(6);
    tagFieldLayout->addWidget(new QLabel("标签", tagField));
    m_tagEditor = new TagSelectorWidget(tagField);
    m_tagEditor->setPlaceholderText("输入新标签，按回车或逗号确认");
    m_tagEditor->setAvailableTags(m_storage.availableTags());
    tagFieldLayout->addWidget(m_tagEditor);

    auto *editorActions = new QHBoxLayout();
    editorActions->setSpacing(10);
    editorActions->addStretch(1);
    editorActions->addWidget(m_cancelEditButton);
    editorActions->addWidget(m_addTodoButton);
    auto *actionsBox = new QWidget(tab);
    auto *actionsBoxLayout = new QVBoxLayout(actionsBox);
    actionsBoxLayout->setContentsMargins(0, 0, 0, 0);
    actionsBoxLayout->addLayout(editorActions);

    bottomRow->addWidget(tagField, 1);
    bottomRow->addWidget(actionsBox, 0, Qt::AlignBottom);

    standardPanelLayout->addLayout(metaRow);
    standardPanelLayout->addLayout(metaSecondaryRow);
    standardPanelLayout->addLayout(bottomRow);
    editorLayout->addWidget(taskField);
    editorLayout->addWidget(standardPanel);
    editorCardLayout->addWidget(m_editorCaptionLabel);
    editorCardLayout->addLayout(editorLayout);

    auto *filterPopup = new QFrame(tab, Qt::Popup | Qt::FramelessWindowHint);
    filterPopup->setObjectName("floatingPanel");
    filterPopup->setAttribute(Qt::WA_TranslucentBackground, true);
    filterPopup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    auto *filterRootLayout = new QVBoxLayout(filterPopup);
    filterRootLayout->setContentsMargins(8, 8, 8, 8);
    filterRootLayout->setSpacing(0);

    auto *filterSurface = new QFrame(filterPopup);
    filterSurface->setObjectName("floatingPanelSurface");
    filterRootLayout->addWidget(filterSurface);

    auto *filterCardLayout = new QVBoxLayout(filterSurface);
    filterCardLayout->setContentsMargins(18, 18, 18, 18);
    filterCardLayout->setSpacing(14);

    auto *filterCaption = new QLabel("视图与筛选", filterSurface);
    filterCaption->setObjectName("sectionTitleSmall");
    auto *filterHint = new QLabel("只影响当前任务列表的显示范围，不会修改任务数据。", filterSurface);
    filterHint->setObjectName("mutedText");
    filterHint->setWordWrap(true);

    auto *filterGrid = new QGridLayout();
    filterGrid->setHorizontalSpacing(12);
    filterGrid->setVerticalSpacing(12);

    auto *viewWrap = new QWidget(filterSurface);
    auto *viewWrapLayout = new QVBoxLayout(viewWrap);
    viewWrapLayout->setContentsMargins(0, 0, 0, 0);
    viewWrapLayout->setSpacing(6);
    auto *viewLabel = new QLabel("视图", viewWrap);
    viewLabel->setObjectName("mutedText");
    m_viewModeFilter = new RoundedComboBox(filterSurface);
    m_viewModeFilter->setObjectName("filterCombo");
    m_viewModeFilter->addItem("今日视图", static_cast<int>(TodoListMode::Today));
    m_viewModeFilter->addItem("全部任务", static_cast<int>(TodoListMode::All));
    m_viewModeFilter->addItem("已完成归档", static_cast<int>(TodoListMode::Archive));
    configureComboBox(m_viewModeFilter, 138);
    m_viewModeFilter->setMinimumHeight(42);
    viewWrapLayout->addWidget(viewLabel);
    viewWrapLayout->addWidget(m_viewModeFilter);

    auto *priorityWrap = new QWidget(filterSurface);
    auto *priorityWrapLayout = new QVBoxLayout(priorityWrap);
    priorityWrapLayout->setContentsMargins(0, 0, 0, 0);
    priorityWrapLayout->setSpacing(6);
    auto *priorityLabel = new QLabel("筛选优先级", priorityWrap);
    priorityLabel->setObjectName("mutedText");
    m_priorityFilter = new RoundedComboBox(filterSurface);
    m_priorityFilter->setObjectName("filterCombo");
    m_priorityFilter->addItem("全部", -1);
    m_priorityFilter->addItem("高", static_cast<int>(TaskPriority::High));
    m_priorityFilter->addItem("中", static_cast<int>(TaskPriority::Medium));
    m_priorityFilter->addItem("低", static_cast<int>(TaskPriority::Low));
    configureComboBox(m_priorityFilter, 108);
    m_priorityFilter->setMinimumHeight(42);
    priorityWrapLayout->addWidget(priorityLabel);
    priorityWrapLayout->addWidget(m_priorityFilter);

    auto *tagWrap = new QWidget(filterSurface);
    auto *tagWrapLayout = new QVBoxLayout(tagWrap);
    tagWrapLayout->setContentsMargins(0, 0, 0, 0);
    tagWrapLayout->setSpacing(6);
    auto *tagLabel = new QLabel("标签", tagWrap);
    tagLabel->setObjectName("mutedText");
    m_tagFilterInput = new TagSelectorWidget(filterSurface);
    m_tagFilterInput->setPlaceholderText("只可选择已有标签");
    m_tagFilterInput->setAvailableTags(m_storage.availableTags());
    m_tagFilterInput->setManualEntryEnabled(false);
    tagWrapLayout->addWidget(tagLabel);
    tagWrapLayout->addWidget(m_tagFilterInput);

    m_clearFilterButton = new QPushButton("清空筛选", filterSurface);
    m_clearFilterButton->setObjectName("secondaryButton");
    m_clearFilterButton->setMinimumHeight(42);

    filterGrid->addWidget(viewWrap, 0, 0);
    filterGrid->addWidget(priorityWrap, 0, 1);
    filterGrid->addWidget(tagWrap, 1, 0);
    filterGrid->addWidget(m_clearFilterButton, 1, 1, Qt::AlignBottom);
    filterCardLayout->addWidget(filterCaption);
    filterCardLayout->addWidget(filterHint);
    filterCardLayout->addLayout(filterGrid);
    m_filterPopup = filterPopup;

    connect(m_filterToggleButton, &QPushButton::clicked, this, [this]() {
        if (m_filterPopup == nullptr || m_filterToggleButton == nullptr) {
            return;
        }

        if (m_filterPopup->isVisible()) {
            m_filterPopup->hide();
            return;
        }

        m_filterPopup->adjustSize();
        const QPoint popupPos = m_filterToggleButton->mapToGlobal(
            QPoint(m_filterToggleButton->width() - m_filterPopup->width(), m_filterToggleButton->height() + 8));
        m_filterPopup->move(popupPos);
        m_filterPopup->show();
        m_filterPopup->raise();
    });
    m_todoList = new QListWidget(content);
    m_todoList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_todoList->setWordWrap(true);
    m_todoList->setSpacing(2);
    m_todoList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_todoList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_todoList->verticalScrollBar()->setSingleStep(18);
    m_todoList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_todoList->installEventFilter(this);
    m_todoList->viewport()->installEventFilter(this);

    auto *planCard = new QFrame(content);
    planCard->setObjectName("surfaceCard");
    planCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *planCardLayout = new QVBoxLayout(planCard);
    planCardLayout->setContentsMargins(14, 14, 14, 14);
    planCardLayout->setSpacing(8);
    auto *planTitle = new QLabel("长期计划", planCard);
    planTitle->setObjectName("sectionTitleSmall");
    auto *planHint = new QLabel("每天会自动生成当天任务，可暂停/恢复。", planCard);
    planHint->setObjectName("mutedText");
    m_planList = new QListWidget(planCard);
    m_planList->setSelectionMode(QAbstractItemView::NoSelection);
    m_planList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_planList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_planList->setSpacing(6);
    m_planList->setMaximumHeight(220);
    m_planList->installEventFilter(this);
    m_planList->viewport()->installEventFilter(this);
    planCardLayout->addWidget(planTitle);
    planCardLayout->addWidget(planHint);
    planCardLayout->addWidget(m_planList);

    layout->addLayout(headerLayout);
    layout->addWidget(m_overdueReminderLabel);
    contentLayout->addWidget(planCard);
    contentLayout->addWidget(m_todoList);
    contentLayout->addWidget(editorCard);
    m_todoStandardHeaderPanel = sidebarCard;
    layout->addLayout(contentLayout, 1);
    layout->addStretch(1);

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    return tab;
}

QWidget *MainWindow::buildPomodoroTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *scrollArea = new QScrollArea(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scrollArea);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(12);

    auto *bindingCard = new QFrame(content);
    bindingCard->setObjectName("surfaceCardSoft");
    bindingCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    bindingCard->setMinimumHeight(180);
    auto *bindingLayout = new QVBoxLayout(bindingCard);
    bindingLayout->setContentsMargins(16, 14, 16, 14);
    bindingLayout->setSpacing(10);

    auto *heroTitle = new QLabel("番茄钟", bindingCard);
    heroTitle->setObjectName("sectionTitle");
    m_cycleHintLabel = new QLabel("开始专注前先绑定当前任务，完成一个专注周期后自动记录到历史。", bindingCard);
    m_cycleHintLabel->setObjectName("mutedText");
    m_cycleHintLabel->setWordWrap(true);

    m_pomodoroTaskSelector = new RoundedComboBox(bindingCard);
    m_pomodoroTaskSelector->setObjectName("pomodoroField");
    m_pomodoroTaskSelector->setMinimumHeight(44);

    m_focusMinutes = new QSpinBox(bindingCard);
    m_focusMinutes->setObjectName("pomodoroField");
    m_focusMinutes->setRange(1, 240);
    m_focusMinutes->setValue(25);
    m_focusMinutes->setMinimumHeight(44);
    m_focusMinutes->setMinimumWidth(150);

    m_breakMinutes = new QSpinBox(bindingCard);
    m_breakMinutes->setObjectName("pomodoroField");
    m_breakMinutes->setRange(1, 120);
    m_breakMinutes->setValue(5);
    m_breakMinutes->setMinimumHeight(44);
    m_breakMinutes->setMinimumWidth(150);

    auto *taskFieldWrap = new QWidget(bindingCard);
    auto *taskField = new QVBoxLayout(taskFieldWrap);
    taskField->setContentsMargins(0, 0, 0, 0);
    taskField->setSpacing(6);
    taskField->addWidget(new QLabel("当前任务", taskFieldWrap));
    taskField->addWidget(m_pomodoroTaskSelector);

    auto *taskTimingWrap = new QWidget(bindingCard);
    auto *taskTimingLayout = new QHBoxLayout(taskTimingWrap);
    taskTimingLayout->setContentsMargins(0, 0, 0, 0);
    taskTimingLayout->setSpacing(12);

    auto *taskTimingInfoWrap = new QWidget(taskTimingWrap);
    auto *taskTimingInfoLayout = new QVBoxLayout(taskTimingInfoWrap);
    taskTimingInfoLayout->setContentsMargins(0, 0, 0, 0);
    taskTimingInfoLayout->setSpacing(4);
    m_pomodoroTaskTimingLabel = new QLabel("累计时长 00:00:00", taskTimingInfoWrap);
    m_pomodoroTaskTimingLabel->setObjectName("sectionTitleSmall");
    taskTimingInfoLayout->addWidget(m_pomodoroTaskTimingLabel);

    m_pomodoroTaskTimingButton = new QPushButton("开始任务计时", taskTimingWrap);
    m_pomodoroTaskTimingButton->setObjectName("secondaryButton");
    m_pomodoroTaskTimingButton->setMinimumHeight(40);

    taskTimingLayout->addWidget(taskTimingInfoWrap, 1);
    taskTimingLayout->addWidget(m_pomodoroTaskTimingButton, 0, Qt::AlignVCenter);

    auto *durationWrap = new QWidget(bindingCard);
    auto *durationRow = new QHBoxLayout(durationWrap);
    durationRow->setContentsMargins(0, 0, 0, 0);
    durationRow->setSpacing(12);

    auto *focusFieldWrap = new QWidget(durationWrap);
    auto *focusField = new QVBoxLayout(focusFieldWrap);
    focusField->setContentsMargins(0, 0, 0, 0);
    focusField->setSpacing(6);
    focusField->addWidget(new QLabel("专注时长", focusFieldWrap));
    focusField->addWidget(m_focusMinutes);

    auto *breakFieldWrap = new QWidget(durationWrap);
    auto *breakField = new QVBoxLayout(breakFieldWrap);
    breakField->setContentsMargins(0, 0, 0, 0);
    breakField->setSpacing(6);
    breakField->addWidget(new QLabel("休息时长", breakFieldWrap));
    breakField->addWidget(m_breakMinutes);

    durationRow->addWidget(focusFieldWrap, 1);
    durationRow->addWidget(breakFieldWrap, 1);

    auto *pomodoroStandardPanel = new QWidget(bindingCard);
    m_pomodoroStandardPanel = pomodoroStandardPanel;
    auto *pomodoroStandardLayout = new QVBoxLayout(pomodoroStandardPanel);
    pomodoroStandardLayout->setContentsMargins(0, 0, 0, 0);
    pomodoroStandardLayout->setSpacing(10);

    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(8);
    auto *presetLabel = new QLabel("预设", bindingCard);
    presetLabel->setObjectName("mutedText");
    m_presetBalancedButton = new QPushButton("标准 25/5", bindingCard);
    m_presetBalancedButton->setObjectName("secondaryButton");
    m_presetDeepFocusButton = new QPushButton("深度 50/10", bindingCard);
    m_presetDeepFocusButton->setObjectName("secondaryButton");
    m_presetQuickButton = new QPushButton("轻量 15/3", bindingCard);
    m_presetQuickButton->setObjectName("secondaryButton");
    presetRow->addWidget(presetLabel);
    presetRow->addWidget(m_presetBalancedButton);
    presetRow->addWidget(m_presetDeepFocusButton);
    presetRow->addWidget(m_presetQuickButton);
    presetRow->addStretch(1);

    m_pomodoroPresetPanel = pomodoroStandardPanel;
    pomodoroStandardLayout->addLayout(presetRow);

    bindingLayout->addWidget(heroTitle);
    bindingLayout->addWidget(m_cycleHintLabel);
    bindingLayout->addWidget(taskFieldWrap);
    bindingLayout->addWidget(taskTimingWrap);
    bindingLayout->addWidget(durationWrap);
    bindingLayout->addWidget(pomodoroStandardPanel);

    auto *dialCard = new QFrame(content);
    dialCard->setObjectName("surfaceCard");
    dialCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    dialCard->setMinimumHeight(420);
    auto *dialLayout = new QVBoxLayout(dialCard);
    dialLayout->setContentsMargins(20, 20, 20, 20);
    dialLayout->setSpacing(0);

    auto *dialStack = new QWidget(dialCard);
    auto *dialStackLayout = new QVBoxLayout(dialStack);
    dialStackLayout->setContentsMargins(0, 0, 0, 0);
    dialStackLayout->setSpacing(16);
    dialStackLayout->setAlignment(Qt::AlignHCenter);

    m_pomodoroDial = new PomodoroDialWidget(dialStack);
    m_pomodoroDial->setFixedSize(220, 220);
    auto *phaseTitle = new QLabel("专注阶段", dialCard);
    phaseTitle->setObjectName("phaseBadge");
    phaseTitle->setAlignment(Qt::AlignCenter);
    m_pomodoroPhaseLabel = phaseTitle;

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(10);
    buttonRow->addStretch(1);
    m_startPauseButton = new QPushButton("开始", dialCard);
    m_startPauseButton->setObjectName("primaryButton");
    m_startPauseButton->setMinimumHeight(38);
    m_stopPomodoroButton = new QPushButton("停止", dialCard);
    m_stopPomodoroButton->setObjectName("secondaryButton");
    m_stopPomodoroButton->setMinimumHeight(38);
    m_resetButton = new QPushButton("重置", dialCard);
    m_resetButton->setObjectName("secondaryButton");
    m_resetButton->setMinimumHeight(38);
    m_pomodoroFocusCardButton = new QPushButton("沉浸卡片", dialCard);
    m_pomodoroFocusCardButton->setObjectName("secondaryButton");
    m_pomodoroFocusCardButton->setMinimumHeight(38);
    buttonRow->addWidget(m_startPauseButton);
    buttonRow->addWidget(m_stopPomodoroButton);
    buttonRow->addWidget(m_resetButton);
    buttonRow->addWidget(m_pomodoroFocusCardButton);
    buttonRow->addStretch(1);

    dialLayout->addStretch(1);
    dialStackLayout->addWidget(m_pomodoroPhaseLabel, 0, Qt::AlignHCenter);
    dialStackLayout->addWidget(m_pomodoroDial, 0, Qt::AlignHCenter);
    dialStackLayout->addLayout(buttonRow);
    dialLayout->addWidget(dialStack, 0, Qt::AlignHCenter);
    dialLayout->addStretch(1);

    auto *historyCard = new QFrame(content);
    m_pomodoroHistoryCard = historyCard;
    historyCard->setObjectName("surfaceCardSoft");
    historyCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    historyCard->setMinimumHeight(120);
    auto *historyLayout = new QVBoxLayout(historyCard);
    historyLayout->setContentsMargins(16, 14, 16, 14);
    historyLayout->setSpacing(8);

    auto *historyTitle = new QLabel("今日番茄历史", historyCard);
    historyTitle->setObjectName("sectionTitleSmall");
    m_focusHistoryList = new QListWidget(historyCard);
    m_focusHistoryList->setObjectName("focusHistoryList");
    m_focusHistoryList->setSelectionMode(QAbstractItemView::NoSelection);
    m_focusHistoryList->setSpacing(6);
    m_focusHistoryList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_focusHistoryList->setMaximumHeight(120);
    historyLayout->addWidget(historyTitle);
    historyLayout->addWidget(m_focusHistoryList);

    contentLayout->addWidget(bindingCard);
    contentLayout->addWidget(dialCard);
    contentLayout->addWidget(historyCard);
    contentLayout->addStretch(1);

    scrollArea->setWidget(content);
    layout->addWidget(scrollArea);

    return tab;
}

QWidget *MainWindow::buildStatsTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *scrollArea = new QScrollArea(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scrollArea);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(28, 28, 28, 28);
    contentLayout->setSpacing(16);
    m_statsContentLayout = contentLayout;

    m_statsHeroLabel = new QLabel(content);
    m_totalTodayLabel = new QLabel(content);
    m_completedTodayLabel = new QLabel(content);
    m_completionRateLabel = new QLabel(content);
    m_focusTodayLabel = new QLabel(content);
    m_focusWeekLabel = new QLabel(content);
    m_taskTimingTodayLabel = new QLabel(content);
    m_activeTaskTimingLabel = new QLabel(content);
    m_statsFocusInsightLabel = new QLabel(content);
    m_statsTimingInsightLabel = new QLabel(content);
    m_statsRankingList = new QListWidget(content);
    m_tagTimingChart = new TagAnalysisChartWidget(content);
    m_tagFocusChart = new TagAnalysisChartWidget(content);

    m_statsHeroLabel->setObjectName("statsHeroCard");
    m_statsFocusInsightLabel->setObjectName("statsNarrativeCard");
    m_statsTimingInsightLabel->setObjectName("statsNarrativeCard");
    m_statsRankingList->setObjectName("statsRankingList");
    m_tagTimingChart->setObjectName("tagAnalysisChart");
    m_tagFocusChart->setObjectName("tagAnalysisChart");

    m_statsHeroLabel->setWordWrap(true);
    m_statsFocusInsightLabel->setWordWrap(true);
    m_statsTimingInsightLabel->setWordWrap(true);
    m_statsHeroLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_statsFocusInsightLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_statsTimingInsightLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_statsRankingList->setSelectionMode(QAbstractItemView::NoSelection);
    m_statsRankingList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_statsRankingList->setMaximumHeight(280);

    auto createMetricCard = [content](const QString &title, QLabel *&valueLabel) {
        auto *card = new QFrame(content);
        card->setObjectName("statsMetricCard");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 16);
        cardLayout->setSpacing(10);

        auto *titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("statsMetricTitle");
        titleLabel->setWordWrap(true);
        titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        valueLabel = new QLabel(card);
        valueLabel->setObjectName("statsMetricValue");
        valueLabel->setWordWrap(true);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        valueLabel->setTextFormat(Qt::PlainText);
        valueLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(valueLabel);
        return card;
    };

    auto *refreshBtn = new QPushButton("刷新统计", content);
    m_statsRefreshButton = refreshBtn;
    refreshBtn->setObjectName("secondaryButton");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshAll);

    m_totalTodayCard = createMetricCard("今日任务", m_totalTodayLabel);
    m_completedTodayCard = createMetricCard("已完成", m_completedTodayLabel);
    m_completionRateCard = createMetricCard("完成率", m_completionRateLabel);
    m_focusTodayCard = createMetricCard("今日番茄", m_focusTodayLabel);
    m_focusWeekCard = createMetricCard("近 7 天番茄", m_focusWeekLabel);
    m_taskTimingTodayCard = createMetricCard("任务计时", m_taskTimingTodayLabel);
    m_activeTaskTimingCard = createMetricCard("当前计时任务", m_activeTaskTimingLabel);

    auto *overviewGrid = new QGridLayout();
    m_statsOverviewGrid = overviewGrid;

    auto *detailGrid = new QGridLayout();
    m_statsDetailGrid = detailGrid;
    rebuildStatsMetricLayout();

    auto *statsStandardPanel = new QWidget(content);
    m_statsStandardPanel = statsStandardPanel;
    auto *statsStandardLayout = new QVBoxLayout(statsStandardPanel);
    statsStandardLayout->setContentsMargins(0, 0, 0, 0);
    statsStandardLayout->setSpacing(16);

    statsStandardLayout->addWidget(m_statsHeroLabel);
    contentLayout->addLayout(overviewGrid);
    contentLayout->addLayout(detailGrid);
    statsStandardLayout->addWidget(m_statsFocusInsightLabel);
    statsStandardLayout->addWidget(m_statsTimingInsightLabel);
    statsStandardLayout->addWidget(m_statsRankingList);
    statsStandardLayout->addWidget(m_tagTimingChart);
    statsStandardLayout->addWidget(m_tagFocusChart);

    auto *storageCard = new QFrame(content);
    storageCard->setObjectName("surfaceCardSoft");
    auto *storageLayout = new QVBoxLayout(storageCard);
    storageLayout->setContentsMargins(16, 14, 16, 14);
    storageLayout->setSpacing(8);
    auto *storageTitle = new QLabel("数据存储位置", storageCard);
    storageTitle->setObjectName("sectionTitleSmall");
    m_storagePathLabel = new QLabel(storageCard);
    m_storagePathLabel->setObjectName("mutedText");
    m_storagePathLabel->setWordWrap(true);
    m_storagePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_storagePathHintLabel = new QLabel(storageCard);
    m_storagePathHintLabel->setObjectName("mutedText");
    m_storagePathHintLabel->setWordWrap(true);
    m_storagePathActionButton = new QPushButton(storageCard);
    m_storagePathActionButton->setObjectName("secondaryButton");
    connect(m_storagePathActionButton, &QPushButton::clicked, this, [this]() {
#if defined(Q_OS_WINDOWS)
        const QString currentPath = m_storage.storageFilePath();
        const QString suggested = QDir(QFileInfo(currentPath).absolutePath()).filePath("data.json");
        const QString selectedPath = QFileDialog::getSaveFileName(
            this,
            "选择数据文件",
            suggested,
            "JSON 文件 (*.json);;所有文件 (*)");
        if (selectedPath.isEmpty()) {
            return;
        }

        if (!m_storage.setStorageFilePath(selectedPath)) {
            showAppWarningDialog(this, "路径更新失败", "无法切换数据文件路径，请确认目录可写后重试。");
            return;
        }

        refreshAll();
        refreshStoragePathUi();
        statusBar()->showMessage("数据文件路径已更新。", 2500);
#else
        refreshStoragePathUi();
#endif
    });
    storageLayout->addWidget(storageTitle);
    storageLayout->addWidget(m_storagePathLabel);
    storageLayout->addWidget(m_storagePathHintLabel);
    storageLayout->addWidget(m_storagePathActionButton, 0, Qt::AlignLeft);
    refreshStoragePathUi();

    contentLayout->addSpacing(16);
    contentLayout->addWidget(statsStandardPanel);
    contentLayout->addWidget(storageCard);
    contentLayout->addWidget(refreshBtn);
    contentLayout->addStretch(1);

    scrollArea->setWidget(content);
    layout->addWidget(scrollArea);

    return tab;
}

void MainWindow::addTodo() {
    const QString title = m_todoInput->text().trimmed();
    if (title.isEmpty()) {
        showAppWarningDialog(this, "保存失败", "任务标题不能为空。");
        return;
    }

    const QDate taskDate = m_taskDateInput->date();
    const bool dailyPlanEnabled = m_dailyPlanEnabled->isChecked();
    const QDate planEndDate = dailyPlanEnabled ? m_planEndDateInput->date() : taskDate;
    if (dailyPlanEnabled && planEndDate < taskDate) {
        showAppWarningDialog(this, "保存失败", "计划结束日期不能早于开始日期。");
        return;
    }

    const auto priority = static_cast<TaskPriority>(m_priorityInput->currentData().toInt());
    const QDateTime dueAt = m_dueAtEnabled->isChecked() ? m_dueAtInput->dateTime() : QDateTime();
    const QStringList tags = collectTags();
    const bool creating = m_editingTodoId.isEmpty();
    const bool savingOutsideSelectedDay = taskDate != m_selectedDate;

    bool saved = false;
    int createdCount = 0;
    if (creating) {
        if (dailyPlanEnabled) {
            saved = m_storage.addDailyPlan(title, taskDate, planEndDate, priority, dueAt, tags, &createdCount);
        } else {
            saved = m_storage.addTodo(title, taskDate, priority, dueAt, tags);
            createdCount = saved ? 1 : 0;
        }
    } else {
        if (dailyPlanEnabled) {
            showAppWarningDialog(this, "编辑限制", "当前编辑模式仅支持单条任务。请先保存当前修改，再使用“每天重复”创建计划任务。");
            return;
        }

        const auto todoOpt = m_storage.todoById(m_editingTodoId);
        if (!todoOpt.has_value()) {
            showAppWarningDialog(this, "保存失败", "当前编辑的任务不存在。");
            resetTodoForm();
            refreshTodoList();
            return;
        }

        TodoItem todo = *todoOpt;
        todo.title = title;
        todo.date = taskDate;
        todo.priority = priority;
        todo.dueAt = dueAt;
        todo.tags = tags;
        saved = m_storage.updateTodo(todo);
    }

    if (!saved) {
        showAppWarningDialog(this, "保存失败", "无法保存任务，请重试。");
        return;
    }

    if (creating && currentTodoListMode() == TodoListMode::Archive) {
        m_viewModeFilter->setCurrentIndex(0);
        m_dateNavigator->setSelectedDate(taskDate);
    } else if (savingOutsideSelectedDay && currentTodoListMode() == TodoListMode::Today) {
        m_dateNavigator->setSelectedDate(taskDate);
        statusBar()->showMessage("已切换到任务日期，方便继续处理这项任务。", 4000);
    }

    if (creating && dailyPlanEnabled) {
        statusBar()->showMessage(
            QString("已创建长期计划，已生成整个区间，共 %1 条任务。").arg(createdCount),
            4500);
    }

    resetTodoForm();
    refreshTodoList();
    refreshPlanList();
    if (creating && m_todoList != nullptr) {
        m_todoList->clearSelection();
    }
    refreshStats();
    refreshTagPresets();
    refreshPomodoroBindings();
}

void MainWindow::cancelTodoEdit() {
    resetTodoForm();
    refreshTodoList();
    refreshPlanList();
}

bool MainWindow::deleteTodoById(const QString &id) {
    if (id.isEmpty()) {
        return false;
    }

    if (!m_storage.removeTodo(id)) {
        showAppWarningDialog(this, "删除失败", "无法删除该任务，请重试。");
        return false;
    }

    if (m_editingTodoId == id) {
        resetTodoForm();
    }

    refreshTodoList();
    refreshStats();
    refreshTagPresets();
    refreshPomodoroBindings();
    updateTaskTimingRefreshState();
    return true;
}

void MainWindow::onTodoItemChanged(QListWidgetItem *item) {
    if (m_refreshingTodoList || item == nullptr) {
        return;
    }

    const QString id = item->data(Qt::UserRole).toString();
    const bool completed = item->checkState() == Qt::Checked;

    if (!m_storage.setTodoCompleted(id, completed)) {
        showAppWarningDialog(this, "更新失败", "无法更新任务状态，请重试。");
        refreshTodoList();
        return;
    }

    refreshTodoList();
    refreshStats();
    refreshTagPresets();
    refreshPomodoroBindings();
}

void MainWindow::onTodoSelectionChanged() {
    if (m_refreshingTodoList) {
        return;
    }

    if (m_todoList == nullptr || m_todoList->selectedItems().isEmpty()) {
        if (!m_editingTodoId.isEmpty()) {
            resetTodoForm();
            refreshTodoList();
        } else {
            updateTodoEditorState();
        }
        return;
    }

    auto *item = m_todoList->currentItem();
    if (item == nullptr) {
        updateTodoEditorState();
        return;
    }

    const QString id = item->data(Qt::UserRole).toString();
    if (id == m_editingTodoId) {
        updateTodoEditorState();
        return;
    }

    const auto todoOpt = m_storage.todoById(id);
    if (!todoOpt.has_value()) {
        resetTodoForm();
        refreshTodoList();
        return;
    }

    populateTodoForm(*todoOpt);
    refreshTodoList();
}

void MainWindow::toggleTodoTiming(const QString &id) {
    if (id.isEmpty()) {
        statusBar()->showMessage("请先在番茄钟里绑定一个任务。", 3000);
        return;
    }

    const auto todoOpt = m_storage.todoById(id);
    if (!todoOpt.has_value()) {
        statusBar()->showMessage("当前任务不存在，无法切换计时。", 3000);
        return;
    }

    const auto activeTodo = m_storage.activeTimedTodo();
    bool success = false;
    QString message;

    if (todoOpt->timingStartedAt.isValid()) {
        success = m_storage.stopTodoTiming(id);
        message = success ? QString("已结束任务计时: %1").arg(todoOpt->title) : "结束任务计时失败，请重试。";
    } else {
        if (m_pomodoroTaskSelectionLocked) {
            statusBar()->showMessage("番茄钟已开始，本轮任务已锁定，不能再切换到任务手动计时。", 4000);
            return;
        }

        if (activeTodo.has_value() && activeTodo->id != id) {
            statusBar()->showMessage(QString("请先结束任务“%1”的计时。").arg(activeTodo->title), 4000);
            return;
        }

        success = m_storage.startTodoTiming(id);
        message = success ? QString("已开始任务计时: %1").arg(todoOpt->title) : "开始任务计时失败，请重试。";
    }

    statusBar()->showMessage(message, success ? 2500 : 4000);
    if (!success) {
        return;
    }

    refreshTodoList();
    refreshStats();
    refreshPomodoroTaskTimingPanel();
    updateTaskTimingRefreshState();
}

void MainWindow::updateTaskTimingRefreshState() {
    if (m_taskTimingRefreshTimer == nullptr) {
        return;
    }

    if (m_storage.hasActiveTodoTiming()) {
        if (!m_taskTimingRefreshTimer->isActive()) {
            m_taskTimingRefreshTimer->start();
        }
    } else {
        m_taskTimingRefreshTimer->stop();
    }
}

void MainWindow::togglePomodoro() {
    if (m_storage.hasActiveTodoTiming()) {
        statusBar()->showMessage("任务手动计时进行中，不能同时开始番茄倒计时。", 4000);
        return;
    }

    if (m_timer.isRunning()) {
        m_timer.pause();
    } else {
        m_pomodoroTaskSelectionLocked = true;
        m_timer.start();
    }

    refreshPomodoroTaskTimingPanel();
}

void MainWindow::resetPomodoro() {
    if (m_storage.hasActiveTodoTiming()) {
        statusBar()->showMessage("任务手动计时进行中，当前时钟由任务计时占用。", 4000);
        return;
    }

    m_timer.reset();
    m_pomodoroTaskSelectionLocked = false;
    refreshPomodoroTaskTimingPanel();
}

void MainWindow::onPomodoroTick(int remainingSeconds, PomodoroTimer::Phase phase) {
    refreshPomodoroView(remainingSeconds, phase);
}

void MainWindow::onPomodoroPhaseCompleted(PomodoroTimer::Phase completedPhase) {
    if (completedPhase == PomodoroTimer::Phase::Focus) {
        const QString taskId = m_pomodoroTaskSelector->currentData().toString();
        if (!taskId.isEmpty() && m_storage.addFocusRecord(taskId, QDateTime::currentDateTime(), m_focusMinutes->value())) {
            statusBar()->showMessage("已完成 1 个专注番茄并计入当前任务", 3000);
        } else if (taskId.isEmpty()) {
            statusBar()->showMessage("当前未绑定任务，本次番茄不会写入任务记录", 3000);
        } else {
            statusBar()->showMessage("番茄完成，但写入任务记录失败", 3000);
        }
        refreshStats();
        refreshPomodoroBindings();
    }
}

void MainWindow::refreshAll() {
    m_storage.ensureDailyPlansGenerated(QDate::currentDate());
    refreshTodoList();
    refreshPlanList();
    refreshStats();
    refreshTagPresets();
    refreshPomodoroBindings();
    refreshPomodoroView(m_timer.remainingSeconds(), m_timer.phase());
    updateTaskTimingRefreshState();
}

void MainWindow::refreshTodoList() {
    const TodoListMode mode = currentTodoListMode();
    m_todayLabel->setText(m_selectedDate.toString("yyyy-MM-dd ddd"));
    m_todayLabel->setToolTip(QString("%1，点击打开日期导航").arg(todoListModeText(mode)));

    const QList<TodoItem> allTodos = m_storage.allTodos();
    QList<TodoItem> scopedTodos;
    scopedTodos.reserve(allTodos.size());
    for (const TodoItem &todo : allTodos) {
        switch (mode) {
        case TodoListMode::Today:
            if (todoCoversDate(todo, m_selectedDate)) {
                scopedTodos.push_back(todo);
            }
            break;
        case TodoListMode::All:
            scopedTodos.push_back(todo);
            break;
        case TodoListMode::Archive:
            if (todo.completed) {
                scopedTodos.push_back(todo);
            }
            break;
        }
    }

    std::sort(scopedTodos.begin(), scopedTodos.end(), [mode](const TodoItem &lhs, const TodoItem &rhs) {
        if (mode == TodoListMode::Archive) {
            const bool lhsHasCompletedAt = lhs.completedAt.isValid();
            const bool rhsHasCompletedAt = rhs.completedAt.isValid();
            if (lhsHasCompletedAt && rhsHasCompletedAt && lhs.completedAt != rhs.completedAt) {
                return lhs.completedAt > rhs.completedAt;
            }
            if (lhs.date != rhs.date) {
                return lhs.date > rhs.date;
            }
            if (lhs.priority != rhs.priority) {
                return static_cast<int>(lhs.priority) > static_cast<int>(rhs.priority);
            }
            return QString::localeAwareCompare(lhs.title, rhs.title) < 0;
        }

        if (lhs.completed != rhs.completed) {
            return !lhs.completed && rhs.completed;
        }

        if (lhs.priority != rhs.priority) {
            return static_cast<int>(lhs.priority) > static_cast<int>(rhs.priority);
        }

        const bool lhsHasDueAt = lhs.dueAt.isValid();
        const bool rhsHasDueAt = rhs.dueAt.isValid();
        if (lhsHasDueAt != rhsHasDueAt) {
            return lhsHasDueAt;
        }

        if (lhsHasDueAt && rhsHasDueAt && lhs.dueAt != rhs.dueAt) {
            return lhs.dueAt < rhs.dueAt;
        }

        if (lhs.date != rhs.date) {
            return lhs.date > rhs.date;
        }

        return QString::localeAwareCompare(lhs.title, rhs.title) < 0;
    });

    QList<TodoItem> visibleTodos;
    visibleTodos.reserve(scopedTodos.size());
    for (const TodoItem &todo : scopedTodos) {
        if (matchesFilters(todo)) {
            visibleTodos.push_back(todo);
        }
    }

    refreshOverdueReminder(allTodos);
    refreshDateSidebar(allTodos);

    const QString selectedId = m_editingTodoId.isEmpty()
                                   ? (m_todoList->currentItem() ? m_todoList->currentItem()->data(Qt::UserRole).toString()
                                                                : QString())
                                   : m_editingTodoId;

    m_refreshingTodoList = true;
    m_todoList->clear();
    QListWidgetItem *selectedItem = nullptr;

    if (visibleTodos.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(m_todoList);
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setSizeHint(QSize(0, scaledMetric(112, 88)));

        auto *emptyCard = new QFrame(m_todoList);
        emptyCard->setObjectName("emptyStateCard");
        auto *emptyLayout = new QVBoxLayout(emptyCard);
        emptyLayout->setContentsMargins(scaledMetric(16, 12), scaledMetric(16, 12),
                                        scaledMetric(16, 12), scaledMetric(16, 12));
        emptyLayout->setSpacing(scaledMetric(6, 4));

        auto *emptyTitle = new QLabel("当前没有可显示的任务", emptyCard);
        emptyTitle->setObjectName("emptyStateTitle");
        emptyTitle->setAlignment(Qt::AlignCenter);

        auto *emptyMeta = new QLabel("可以在上方新建任务，或调整视图与筛选条件。", emptyCard);
        emptyMeta->setObjectName("emptyStateMeta");
        emptyMeta->setAlignment(Qt::AlignCenter);
        emptyMeta->setWordWrap(true);

        emptyLayout->addStretch(1);
        emptyLayout->addWidget(emptyTitle);
        emptyLayout->addWidget(emptyMeta);
        emptyLayout->addStretch(1);
        installEventFilterOnWidgetTree(this, emptyCard);
        m_todoList->setItemWidget(emptyItem, emptyCard);
    } else {
        const int itemWidth = qMax(0, m_todoList->viewport()->width() - 6);
        for (const TodoItem &todo : visibleTodos) {
            auto *item = new QListWidgetItem(m_todoList);
            item->setData(Qt::UserRole, todo.id);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            QWidget *card = createTodoCard(todo);
            m_todoList->setItemWidget(item, card);
            int cardHeight = card->sizeHint().height();
            if (QLayout *layout = card->layout(); layout != nullptr && itemWidth > 0) {
                layout->activate();
                cardHeight = layout->hasHeightForWidth()
                                 ? layout->totalHeightForWidth(itemWidth)
                                 : layout->sizeHint().height();
            }
            item->setSizeHint(QSize(itemWidth, cardHeight));

            if (!selectedId.isEmpty() && todo.id == selectedId) {
                selectedItem = item;
            }
        }
    }

    m_refreshingTodoList = false;

    if (selectedItem != nullptr) {
        m_todoList->setCurrentItem(selectedItem);
    } else if (!m_editingTodoId.isEmpty()) {
        resetTodoForm();
    }

    if (m_todoList != nullptr) {
        int contentHeight = 0;
        for (int i = 0; i < m_todoList->count(); ++i) {
            if (QListWidgetItem *item = m_todoList->item(i); item != nullptr) {
                contentHeight += item->sizeHint().height();
            }
        }
        if (m_todoList->count() > 1) {
            contentHeight += (m_todoList->count() - 1) * m_todoList->spacing();
        }

        const int framePadding = scaledMetric(10, 8);
        const int minHeight = visibleTodos.isEmpty() ? scaledMetric(124, 96) : 0;
        const int maxHeight = qMax(scaledMetric(180, 140), qRound(height() * 0.24));
        const int targetHeight = qBound(minHeight, contentHeight + framePadding, maxHeight);
        m_todoList->setFixedHeight(targetHeight);
        m_todoList->setVerticalScrollBarPolicy(targetHeight >= maxHeight
                                                   ? Qt::ScrollBarAsNeeded
                                                   : Qt::ScrollBarAlwaysOff);
    }

    updateTodoEditorState();
}

void MainWindow::refreshPlanList() {
    if (m_planList == nullptr) {
        return;
    }

    const QList<TodoPlan> plans = m_storage.allPlans();
    m_planList->clear();
    if (plans.isEmpty()) {
        m_planList->addItem("暂无长期计划");
    } else {
        QList<TodoPlan> sortedPlans = plans;
        std::sort(sortedPlans.begin(), sortedPlans.end(), [](const TodoPlan &lhs, const TodoPlan &rhs) {
            if (lhs.paused != rhs.paused) {
                return !lhs.paused && rhs.paused;
            }
            if (lhs.startDate != rhs.startDate) {
                return lhs.startDate < rhs.startDate;
            }
            return QString::localeAwareCompare(lhs.title, rhs.title) < 0;
        });

        for (const TodoPlan &plan : sortedPlans) {
            auto *item = new QListWidgetItem(m_planList);
            item->setFlags(Qt::ItemIsEnabled);
            item->setSizeHint(QSize(0, scaledMetric(64, 52)));

            auto *card = new QFrame(m_planList);
            card->setObjectName("surfaceCardSoft");
            auto *layout = new QHBoxLayout(card);
            layout->setContentsMargins(scaledMetric(8, 6), scaledMetric(6, 4),
                                       scaledMetric(8, 6), scaledMetric(6, 4));
            layout->setSpacing(scaledMetric(6, 4));

            auto *textWrap = new QVBoxLayout();
            textWrap->setContentsMargins(0, 0, 0, 0);
            textWrap->setSpacing(2);
            const QString title = QString("%1%2")
                                      .arg(plan.paused ? "[暂停] " : "[进行中] ")
                                      .arg(plan.title);
            auto *titleLabel = new QLabel(title, card);
            titleLabel->setObjectName("sectionTitleSmall");
            auto *metaLabel = new QLabel(
                QString("%1 ~ %2%3")
                    .arg(plan.startDate.toString("yyyy-MM-dd"))
                    .arg(plan.endDate.toString("yyyy-MM-dd"))
                    .arg(plan.dueTime.isValid() ? QString(" · 截止 %1").arg(plan.dueTime.toString("HH:mm")) : QString()),
                card);
            metaLabel->setObjectName("mutedText");
            textWrap->addWidget(titleLabel);
            textWrap->addWidget(metaLabel);

        auto *toggleButton = new QPushButton(plan.paused ? "恢复" : "暂停", card);
        toggleButton->setObjectName("secondaryButton");
        toggleButton->setMinimumHeight(scaledMetric(30, 24));

        auto *editButton = new QPushButton("编辑", card);
        editButton->setObjectName("secondaryButton");
        editButton->setMinimumHeight(scaledMetric(30, 24));

        auto *removeButton = new QPushButton("删除", card);
        removeButton->setObjectName("secondaryButton");
        removeButton->setMinimumHeight(scaledMetric(30, 24));

        layout->addLayout(textWrap, 1);
        layout->addWidget(toggleButton, 0, Qt::AlignVCenter);
        layout->addWidget(editButton, 0, Qt::AlignVCenter);
        layout->addWidget(removeButton, 0, Qt::AlignVCenter);

            installEventFilterOnWidgetTree(this, card);
            m_planList->setItemWidget(item, card);

        connect(toggleButton, &QPushButton::clicked, this, [this, plan]() {
            if (!m_storage.setPlanPaused(plan.id, !plan.paused)) {
                showAppWarningDialog(this, "操作失败", "无法更新计划状态，请重试。");
                return;
            }
            refreshAll();
        });

        connect(editButton, &QPushButton::clicked, this, [this, plan]() {
            QDialog dialog(this);
            dialog.setObjectName("planEditDialog");
            dialog.setWindowTitle("编辑长期计划");
            dialog.setModal(true);
            dialog.setFixedWidth(420);
            dialog.setStyleSheet(R"(
                QDialog#planEditDialog {
                    background: #f7f7f7;
                }
                QFrame#planEditPanel {
                    background: #ffffff;
                    border: 1px solid #e8e8e8;
                    border-radius: 20px;
                }
                QLabel#planEditTitle {
                    color: #191919;
                    font-size: 18px;
                    font-weight: 700;
                }
                QLabel#planEditHint {
                    color: #6b7280;
                    font-size: 13px;
                }
                QLabel#planFieldLabel {
                    color: #2f3437;
                    font-size: 13px;
                    font-weight: 600;
                }
            )");

            auto *rootLayout = new QVBoxLayout(&dialog);
            rootLayout->setContentsMargins(12, 12, 12, 12);
            rootLayout->setSpacing(0);

            auto *panel = new QFrame(&dialog);
            panel->setObjectName("planEditPanel");
            rootLayout->addWidget(panel);

            auto *layout = new QVBoxLayout(panel);
            layout->setContentsMargins(18, 18, 18, 18);
            layout->setSpacing(12);

            auto *headerTitle = new QLabel("编辑长期计划", panel);
            headerTitle->setObjectName("planEditTitle");
            auto *headerHint = new QLabel("修改计划模板，可选择是否同步历史未完成任务。", panel);
            headerHint->setObjectName("planEditHint");
            headerHint->setWordWrap(true);

            auto *titleInput = new QLineEdit(panel);
            titleInput->setText(plan.title);
            titleInput->setPlaceholderText("计划标题");
            titleInput->setMinimumHeight(40);

            auto *startInput = new QDateEdit(plan.startDate, panel);
            startInput->setDisplayFormat("yyyy-MM-dd");
            startInput->setCalendarPopup(true);
            startInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
            startInput->setMinimumHeight(40);

            auto *endInput = new QDateEdit(plan.endDate, panel);
            endInput->setDisplayFormat("yyyy-MM-dd");
            endInput->setCalendarPopup(true);
            endInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
            endInput->setMinimumHeight(40);

            auto *priorityInput = new RoundedComboBox(panel);
            priorityInput->addItem("高", static_cast<int>(TaskPriority::High));
            priorityInput->addItem("中", static_cast<int>(TaskPriority::Medium));
            priorityInput->addItem("低", static_cast<int>(TaskPriority::Low));
            priorityInput->setMinimumHeight(40);
            for (int idx = 0; idx < priorityInput->count(); ++idx) {
                if (priorityInput->itemData(idx).toInt() == static_cast<int>(plan.priority)) {
                    priorityInput->setCurrentIndex(idx);
                    break;
                }
            }

            auto *dueEnabled = new QCheckBox("启用截止时间", panel);
            dueEnabled->setChecked(plan.dueTime.isValid());
            auto *dueInput = new QTimeEdit(panel);
            dueInput->setDisplayFormat("HH:mm");
            dueInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
            dueInput->setTime(plan.dueTime.isValid() ? plan.dueTime : QTime(18, 0));
            dueInput->setEnabled(dueEnabled->isChecked());
            dueInput->setMinimumHeight(40);
            installWheelBlocker(dueInput, true);
            connect(dueEnabled, &QCheckBox::toggled, dueInput, &QWidget::setEnabled);

            auto *tagEditor = new TagSelectorWidget(panel);
            tagEditor->setPlaceholderText("输入新标签，按回车或逗号确认");
            tagEditor->setAvailableTags(m_storage.availableTags());
            tagEditor->setSelectedTags(plan.tags);

            auto *syncPastBox = new QCheckBox("同步历史未完成任务", panel);
            syncPastBox->setChecked(false);
            auto *syncPastHint = new QLabel("不勾选：只更新今天及未来未完成任务。勾选：连过去未完成的计划任务一起更新。", panel);
            syncPastHint->setObjectName("planEditHint");
            syncPastHint->setWordWrap(true);

            auto addField = [panel, layout](const QString &labelText, QWidget *field) {
                auto *label = new QLabel(labelText, panel);
                label->setObjectName("planFieldLabel");
                layout->addWidget(label);
                layout->addWidget(field);
            };

            layout->addWidget(headerTitle);
            layout->addWidget(headerHint);
            addField("标题", titleInput);

            auto *dateRow = new QHBoxLayout();
            dateRow->setContentsMargins(0, 0, 0, 0);
            dateRow->setSpacing(10);
            auto *startWrap = new QWidget(panel);
            auto *startWrapLayout = new QVBoxLayout(startWrap);
            startWrapLayout->setContentsMargins(0, 0, 0, 0);
            startWrapLayout->setSpacing(6);
            auto *startLabel = new QLabel("开始日期", startWrap);
            startLabel->setObjectName("planFieldLabel");
            startWrapLayout->addWidget(startLabel);
            startWrapLayout->addWidget(startInput);
            auto *endWrap = new QWidget(panel);
            auto *endWrapLayout = new QVBoxLayout(endWrap);
            endWrapLayout->setContentsMargins(0, 0, 0, 0);
            endWrapLayout->setSpacing(6);
            auto *endLabel = new QLabel("结束日期", endWrap);
            endLabel->setObjectName("planFieldLabel");
            endWrapLayout->addWidget(endLabel);
            endWrapLayout->addWidget(endInput);
            dateRow->addWidget(startWrap, 1);
            dateRow->addWidget(endWrap, 1);
            layout->addLayout(dateRow);

            auto *priorityDueRow = new QHBoxLayout();
            priorityDueRow->setContentsMargins(0, 0, 0, 0);
            priorityDueRow->setSpacing(10);
            auto *priorityWrap = new QWidget(panel);
            auto *priorityWrapLayout = new QVBoxLayout(priorityWrap);
            priorityWrapLayout->setContentsMargins(0, 0, 0, 0);
            priorityWrapLayout->setSpacing(6);
            auto *priorityLabel = new QLabel("优先级", priorityWrap);
            priorityLabel->setObjectName("planFieldLabel");
            priorityWrapLayout->addWidget(priorityLabel);
            priorityWrapLayout->addWidget(priorityInput);
            auto *dueWrap = new QWidget(panel);
            auto *dueWrapLayout = new QVBoxLayout(dueWrap);
            dueWrapLayout->setContentsMargins(0, 0, 0, 0);
            dueWrapLayout->setSpacing(6);
            dueWrapLayout->addWidget(dueEnabled);
            dueWrapLayout->addWidget(dueInput);
            priorityDueRow->addWidget(priorityWrap, 1);
            priorityDueRow->addWidget(dueWrap, 1);
            layout->addLayout(priorityDueRow);

            addField("标签", tagEditor);
            layout->addWidget(syncPastBox);
            layout->addWidget(syncPastHint);

            auto *actions = new QHBoxLayout();
            actions->setContentsMargins(0, 6, 0, 0);
            actions->setSpacing(10);
            actions->addStretch(1);
            auto *cancelBtn = new QPushButton("取消", panel);
            cancelBtn->setObjectName("secondaryButton");
            cancelBtn->setMinimumHeight(42);
            cancelBtn->setMinimumWidth(96);
            auto *saveBtn = new QPushButton("保存", panel);
            saveBtn->setObjectName("primaryButton");
            saveBtn->setMinimumHeight(42);
            saveBtn->setMinimumWidth(96);
            actions->addWidget(cancelBtn);
            actions->addWidget(saveBtn);
            layout->addLayout(actions);

            connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
            connect(saveBtn, &QPushButton::clicked, &dialog, [&dialog]() { dialog.accept(); });

            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            const QString updatedTitle = titleInput->text().trimmed();
            const QDate updatedStart = startInput->date();
            const QDate updatedEnd = endInput->date();
            if (updatedTitle.isEmpty()) {
                showAppWarningDialog(this, "保存失败", "计划标题不能为空。");
                return;
            }
            if (!updatedStart.isValid() || !updatedEnd.isValid() || updatedEnd < updatedStart) {
                showAppWarningDialog(this, "保存失败", "计划结束日期不能早于开始日期。");
                return;
            }

            TodoPlan updated = plan;
            updated.title = updatedTitle;
            updated.startDate = updatedStart;
            updated.endDate = updatedEnd;
            updated.priority = static_cast<TaskPriority>(priorityInput->currentData().toInt());
            updated.dueTime = dueEnabled->isChecked() ? dueInput->time() : QTime();
            updated.tags = tagEditor->selectedTags();

            if (!m_storage.updatePlan(updated, syncPastBox->isChecked())) {
                showAppWarningDialog(this, "保存失败", "无法更新计划，请重试。");
                return;
            }

            statusBar()->showMessage("长期计划已更新。", 2500);
            refreshAll();
        });

            connect(removeButton, &QPushButton::clicked, this, [this, plan]() {
                const bool confirmed = showAppDialog(
                    this,
                    AppDialogKind::Confirm,
                    "删除长期计划",
                    QString("删除计划“%1”后，将同时删除该计划自动生成的任务。是否继续？").arg(plan.title),
                    "删除",
                    "取消");
                if (!confirmed) {
                    return;
                }
                if (!m_storage.removePlan(plan.id)) {
                    showAppWarningDialog(this, "删除失败", "无法删除计划，请重试。");
                    return;
                }
                refreshAll();
            });
        }
    }

    int contentHeight = 0;
    for (int i = 0; i < m_planList->count(); ++i) {
        if (QListWidgetItem *item = m_planList->item(i); item != nullptr) {
            contentHeight += item->sizeHint().height();
        }
    }
    if (m_planList->count() > 1) {
        contentHeight += (m_planList->count() - 1) * m_planList->spacing();
    }

    const int framePadding = scaledMetric(6, 4);
    const int minHeight = scaledMetric(42, 34);
    const int maxHeight = qMax(scaledMetric(108, 84), qRound(height() * 0.16));
    const int targetHeight = qBound(minHeight, contentHeight + framePadding, maxHeight);
    m_planList->setFixedHeight(targetHeight);
    m_planList->setVerticalScrollBarPolicy(targetHeight >= maxHeight
                                               ? Qt::ScrollBarAsNeeded
                                               : Qt::ScrollBarAlwaysOff);
}

void MainWindow::updateTodoItemSizeHints() {
    return;
}

void MainWindow::refreshStats() {
    const QDate statsDate = m_selectedDate;
    const TodoStats stats = m_storage.currentStats(statsDate);
    const auto activeTodo = m_storage.activeTimedTodo();
    const QList<TodoItem> allTodos = m_storage.allTodos();
    const QList<TodoItem> dailyTodos = m_storage.todosForDate(statsDate);
    struct RankedTask {
        QString title;
        qint64 trackedSeconds = 0;
        int focusCount = 0;
        bool completed = false;
    };
    struct TagAggregate {
        qint64 trackedSeconds = 0;
        int focusCount = 0;
    };
    QList<RankedTask> rankedTasks;
    QMap<QString, TagAggregate> tagAggregates;

    const double rate = (stats.totalToday == 0)
                            ? 0.0
                            : static_cast<double>(stats.completedToday) * 100.0 / static_cast<double>(stats.totalToday);
    int activeToday = 0;
    int overdueToday = 0;
    int highPriorityToday = 0;
    int focusedTaskCount = 0;
    int topTaskFocusCount = 0;
    qint64 topTaskTrackedSeconds = 0;
    QString topFocusTask = "暂无";
    QString topTimingTask = "暂无";
    int focusMinutesToday = 0;
    int focusMinutesWeek = 0;

    for (const TodoItem &item : dailyTodos) {
        if (!item.completed) {
            ++activeToday;
        }
        if (isOverdueToday(item)) {
            ++overdueToday;
        }
        if (item.priority == TaskPriority::High) {
            ++highPriorityToday;
        }
    }

    for (const TodoItem &item : allTodos) {
        if (!item.focusRecords.isEmpty()) {
            ++focusedTaskCount;
        }

        int taskFocusToday = 0;
        int taskFocusTotal = item.focusRecords.size();
        for (const TodoItem::FocusRecord &record : item.focusRecords) {
            if (record.completedAt.date() == statsDate) {
                ++taskFocusToday;
                focusMinutesToday += record.focusMinutes;
            }
            if (record.completedAt.date() >= statsDate.addDays(-6)
                && record.completedAt.date() <= statsDate) {
                focusMinutesWeek += record.focusMinutes;
            }
        }

        if (taskFocusToday > topTaskFocusCount) {
            topTaskFocusCount = taskFocusToday;
            topFocusTask = item.title;
        }

        const qint64 tracked = TaskStorage::effectiveTrackedSeconds(item, QDateTime::currentDateTime());
        if (tracked > topTaskTrackedSeconds) {
            topTaskTrackedSeconds = tracked;
            topTimingTask = item.title;
        }

        for (const QString &tag : item.tags) {
            TagAggregate &aggregate = tagAggregates[tag];
            aggregate.trackedSeconds += tracked;
            aggregate.focusCount += taskFocusTotal;
        }

        if (tracked > 0 || taskFocusTotal > 0) {
            RankedTask rankedTask;
            rankedTask.title = item.title;
            rankedTask.trackedSeconds = tracked;
            rankedTask.focusCount = taskFocusTotal;
            rankedTask.completed = item.completed;
            rankedTasks.push_back(rankedTask);
        }
    }

    std::sort(rankedTasks.begin(), rankedTasks.end(), [](const RankedTask &lhs, const RankedTask &rhs) {
        if (lhs.trackedSeconds != rhs.trackedSeconds) {
            return lhs.trackedSeconds > rhs.trackedSeconds;
        }
        if (lhs.focusCount != rhs.focusCount) {
            return lhs.focusCount > rhs.focusCount;
        }
        return QString::localeAwareCompare(lhs.title, rhs.title) < 0;
    });

    QList<TagAnalysisChartWidget::BarItem> tagTimingBars;
    QList<TagAnalysisChartWidget::BarItem> tagFocusBars;
    QStringList tagNames = tagAggregates.keys();
    std::sort(tagNames.begin(), tagNames.end(), [&tagAggregates](const QString &lhs, const QString &rhs) {
        if (tagAggregates[lhs].trackedSeconds != tagAggregates[rhs].trackedSeconds) {
            return tagAggregates[lhs].trackedSeconds > tagAggregates[rhs].trackedSeconds;
        }
        return QString::localeAwareCompare(lhs, rhs) < 0;
    });
    for (int i = 0; i < qMin(6, tagNames.size()); ++i) {
        const QString &tag = tagNames.at(i);
        TagAnalysisChartWidget::BarItem item;
        item.label = tag;
        item.value = static_cast<double>(tagAggregates[tag].trackedSeconds);
        item.displayValue = formatDuration(tagAggregates[tag].trackedSeconds);
        tagTimingBars.push_back(item);
    }
    std::sort(tagNames.begin(), tagNames.end(), [&tagAggregates](const QString &lhs, const QString &rhs) {
        if (tagAggregates[lhs].focusCount != tagAggregates[rhs].focusCount) {
            return tagAggregates[lhs].focusCount > tagAggregates[rhs].focusCount;
        }
        return QString::localeAwareCompare(lhs, rhs) < 0;
    });
    for (int i = 0; i < qMin(6, tagNames.size()); ++i) {
        const QString &tag = tagNames.at(i);
        if (tagAggregates[tag].focusCount <= 0) {
            continue;
        }
        TagAnalysisChartWidget::BarItem item;
        item.label = tag;
        item.value = static_cast<double>(tagAggregates[tag].focusCount);
        item.displayValue = QString("%1 个").arg(tagAggregates[tag].focusCount);
        tagFocusBars.push_back(item);
    }

    m_statsHeroLabel->setText(
        QString("今日概览\n共 %1 项任务，完成 %2 项，剩余 %3 项，逾期 %4 项。高优先级任务 %5 项，整体完成率 %6%。")
            .arg(stats.totalToday)
            .arg(stats.completedToday)
            .arg(activeToday)
            .arg(overdueToday)
            .arg(highPriorityToday)
            .arg(QString::number(rate, 'f', 1)));

    m_totalTodayLabel->setText(QString("%1 项").arg(stats.totalToday));
    m_completedTodayLabel->setText(QString("%1 项").arg(stats.completedToday));
    m_completionRateLabel->setText(QString("%1%").arg(QString::number(rate, 'f', 1)));
    m_focusTodayLabel->setText(QString("%1 个").arg(stats.focusToday));
    m_focusWeekLabel->setText(QString("%1 个\n%2 分钟").arg(stats.focusLast7Days).arg(focusMinutesWeek));
    m_taskTimingTodayLabel->setText(
        QString("今日 %1\n累计 %2")
            .arg(formatDuration(stats.trackedSecondsToday))
            .arg(formatDuration(stats.trackedSecondsAll)));

    if (activeTodo.has_value()) {
        m_activeTaskTimingLabel->setText(
            QString("%1\n已持续 %2")
                .arg(activeTodo->title, formatDuration(TaskStorage::effectiveTrackedSeconds(*activeTodo, QDateTime::currentDateTime()))));
    } else {
        m_activeTaskTimingLabel->setText("无");
    }

    m_statsFocusInsightLabel->setText(
        QString("专注洞察\n今日累计专注 %1 分钟，覆盖 %2 个任务。当前最聚焦任务：%3（今日 %4 个番茄）。")
            .arg(focusMinutesToday)
            .arg(focusedTaskCount)
            .arg(topFocusTask)
            .arg(topTaskFocusCount));
    m_statsTimingInsightLabel->setText(
        QString("时间洞察\n当前累计投入时长最多的任务：%1（%2）。这能帮助你判断精力是否投入在真正重要的任务上。")
            .arg(topTimingTask)
            .arg(formatDuration(topTaskTrackedSeconds)));

    if (m_statsRankingList != nullptr) {
        m_statsRankingList->clear();
        if (rankedTasks.isEmpty()) {
            m_statsRankingList->addItem("任务排行榜\n暂无足够数据，先开始计时或完成几个番茄。");
        } else {
            const int limit = qMin(5, rankedTasks.size());
            for (int i = 0; i < limit; ++i) {
                const RankedTask &task = rankedTasks[i];
                QString line = QString("#%1  %2\n累计 %3 · 番茄 %4 个")
                                   .arg(i + 1)
                                   .arg(task.title)
                                   .arg(formatDuration(task.trackedSeconds))
                                   .arg(task.focusCount);
                if (task.completed) {
                    line += " · 已完成";
                }
                m_statsRankingList->addItem(line);
            }
        }
    }

    if (m_tagTimingChart != nullptr) {
        m_tagTimingChart->setChartData("标签分析 · 累计投入时长", "带标签的任务还不够多，暂无可分析数据。", tagTimingBars);
    }
    if (m_tagFocusChart != nullptr) {
        m_tagFocusChart->setChartData("标签分析 · 番茄数量", "当前还没有带标签的番茄记录。", tagFocusBars);
    }
    refreshStoragePathUi();
}

void MainWindow::refreshStoragePathUi() {
    if (m_storagePathLabel == nullptr || m_storagePathHintLabel == nullptr || m_storagePathActionButton == nullptr) {
        return;
    }

    const QString path = QDir::toNativeSeparators(m_storage.storageFilePath());
    m_storagePathLabel->setText(path);
#if defined(Q_OS_WINDOWS)
    m_storagePathHintLabel->setText("Windows：支持自定义数据文件路径，修改后会自动保存并立即生效。");
    m_storagePathActionButton->setText("更改路径");
    m_storagePathActionButton->setEnabled(true);
#else
    m_storagePathHintLabel->setText("Linux：当前版本仅支持查看存储路径。");
    m_storagePathActionButton->setText("仅查看");
    m_storagePathActionButton->setEnabled(false);
#endif
}

void MainWindow::refreshTaskTimingUi() {
    if (!m_storage.hasActiveTodoTiming()) {
        updateTaskTimingRefreshState();
        return;
    }

    refreshPomodoroTaskTimingPanel();
    refreshPomodoroView(m_timer.remainingSeconds(), m_timer.phase());

    if (m_tabs != nullptr && m_tabs->currentIndex() == 2) {
        refreshStats();
    }
}

void MainWindow::refreshPomodoroView(int remainingSeconds, PomodoroTimer::Phase phase) {
    const auto activeTodo = m_storage.activeTimedTodo();
    if (activeTodo.has_value()) {
        const qint64 trackedSeconds = TaskStorage::effectiveTrackedSeconds(*activeTodo, QDateTime::currentDateTime());
        const QString timeText = formatDuration(trackedSeconds);
        m_pomodoroDial->setTimeText(timeText);
        m_pomodoroDial->setProgress(0.0);
        m_pomodoroPhaseLabel->setText("任务计时");
        syncPomodoroFocusCard("任务计时", timeText, 0.0);
        m_cycleHintLabel->setText(QString("当前正在为“%1”累计时长，结束前不能开始番茄倒计时或切换任务。").arg(activeTodo->title));
        return;
    }

    const int totalSeconds = (phase == PomodoroTimer::Phase::Focus ? m_focusMinutes->value() : m_breakMinutes->value()) * 60;
    const int elapsedSeconds = qMax(0, totalSeconds - remainingSeconds);
    const double progress = totalSeconds > 0
                                ? (static_cast<double>(elapsedSeconds) * 100.0 / static_cast<double>(totalSeconds))
                                : 0.0;

    const QString timeText = formatSeconds(remainingSeconds);
    m_pomodoroDial->setTimeText(timeText);
    m_pomodoroDial->setProgress(progress);

    if (phase == PomodoroTimer::Phase::Focus) {
        m_pomodoroPhaseLabel->setText("专注阶段");
        syncPomodoroFocusCard("专注阶段", timeText, progress);
        m_cycleHintLabel->setText("专注阶段会累计番茄统计，适合连续推进重要任务。");
    } else {
        m_pomodoroPhaseLabel->setText("休息阶段");
        syncPomodoroFocusCard("休息阶段", timeText, progress);
        m_cycleHintLabel->setText("休息阶段用于短暂恢复，结束后会自动切回专注。");
    }
}

void MainWindow::togglePomodoroFocusCard() {
    if (m_pomodoroFocusCard != nullptr && m_pomodoroFocusCard->isVisible()) {
        hidePomodoroFocusCard(true);
        return;
    }

    showPomodoroFocusCard();
}

void MainWindow::showPomodoroFocusCard() {
    if (m_pomodoroFocusCard == nullptr) {
        m_pomodoroFocusCard = new PomodoroFocusCard();
        m_pomodoroFocusCard->setWindowIcon(windowIcon());
        connect(m_pomodoroFocusCard, &PomodoroFocusCard::exitRequested, this, [this]() {
            hidePomodoroFocusCard(true);
        });
        connect(m_pomodoroFocusCard, &PomodoroFocusCard::positionChanged, this, [this](const QPoint &topLeft) {
            m_lastFocusCardTopLeft = topLeft;
        });
    }

    refreshPomodoroView(m_timer.remainingSeconds(), m_timer.phase());

    if (!m_pomodoroFocusCard->isVisible()) {
        const QSize cardSize = m_pomodoroFocusCard->size();
        QPoint targetTopLeft;
        if (isVisible() && !isMinimized()) {
            targetTopLeft = QPoint(
                frameGeometry().center().x() - cardSize.width() / 2,
                frameGeometry().center().y() - cardSize.height() / 2);
        } else if (m_lastFocusCardTopLeft.x() >= 0 && m_lastFocusCardTopLeft.y() >= 0) {
            targetTopLeft = m_lastFocusCardTopLeft;
        } else if (QScreen *screenHandle = screen(); screenHandle != nullptr) {
            const QRect available = screenHandle->availableGeometry();
            targetTopLeft = QPoint(
                available.center().x() - cardSize.width() / 2,
                available.center().y() - cardSize.height() / 2);
        } else {
            targetTopLeft = QPoint(
                frameGeometry().center().x() - cardSize.width() / 2,
                frameGeometry().center().y() - cardSize.height() / 2);
        }
        m_pomodoroFocusCard->move(targetTopLeft);
        m_lastFocusCardTopLeft = targetTopLeft;
    }

    m_pomodoroFocusCard->show();
    m_pomodoroFocusCard->raise();
    m_pomodoroFocusCard->activateWindow();
    updatePomodoroFocusCardButton();
    hide();
    updateTrayActions();
}

void MainWindow::hidePomodoroFocusCard(bool restoreMainWindow) {
    if (m_pomodoroFocusCard != nullptr) {
        m_lastFocusCardTopLeft = m_pomodoroFocusCard->pos();
        m_pomodoroFocusCard->hide();
    }

    updatePomodoroFocusCardButton();
    if (restoreMainWindow) {
        if (m_tabs != nullptr) {
            m_tabs->setCurrentIndex(1);
        }
        move(restoredMainWindowTopLeft());
        showNormal();
        raise();
        activateWindow();
    }
}

void MainWindow::syncPomodoroFocusCard(const QString &phaseText, const QString &timeText, double progress) {
    if (m_pomodoroFocusCard == nullptr) {
        return;
    }

    m_pomodoroFocusCard->setPhaseText(phaseText);
    m_pomodoroFocusCard->setTimeText(timeText);
    m_pomodoroFocusCard->setProgress(progress);
}

void MainWindow::updatePomodoroFocusCardButton() {
    if (m_pomodoroFocusCardButton == nullptr) {
        return;
    }

    m_pomodoroFocusCardButton->setText(
        m_pomodoroFocusCard != nullptr && m_pomodoroFocusCard->isVisible() ? "关闭沉浸卡片" : "沉浸卡片");
}

QPoint MainWindow::restoredMainWindowTopLeft() const {
    if (m_pomodoroFocusCard == nullptr && (m_lastFocusCardTopLeft.x() < 0 || m_lastFocusCardTopLeft.y() < 0)) {
        return pos();
    }

    const QPoint focusCardTopLeft = m_pomodoroFocusCard != nullptr && m_pomodoroFocusCard->isVisible()
                                        ? m_pomodoroFocusCard->pos()
                                        : m_lastFocusCardTopLeft;
    if (focusCardTopLeft.x() < 0 || focusCardTopLeft.y() < 0) {
        return pos();
    }

    const QSize mainSize = frameGeometry().size();
    const QSize focusCardSize = m_pomodoroFocusCard != nullptr ? m_pomodoroFocusCard->size() : QSize(260, 300);
    return QPoint(
        focusCardTopLeft.x() + focusCardSize.width() / 2 - mainSize.width() / 2,
        focusCardTopLeft.y() + focusCardSize.height() / 2 - mainSize.height() / 2);
}

void MainWindow::refreshPomodoroBindings() {
    if (m_pomodoroTaskSelector != nullptr) {
        const QString currentTaskId = m_pomodoroTaskSelector->currentData().toString();
        const QList<TodoItem> todayTodos = m_storage.todosForDate(QDate::currentDate());

        m_pomodoroTaskSelector->blockSignals(true);
        m_pomodoroTaskSelector->clear();
        m_pomodoroTaskSelector->addItem("不绑定任务", QString());

        for (const TodoItem &todo : todayTodos) {
            if (todo.completed) {
                continue;
            }

            const QString label = QString("%1 · %2").arg(todo.title, todo.date.toString("MM-dd"));
            m_pomodoroTaskSelector->addItem(label, todo.id);
        }

        int restoredIndex = 0;
        for (int i = 0; i < m_pomodoroTaskSelector->count(); ++i) {
            if (m_pomodoroTaskSelector->itemData(i).toString() == currentTaskId) {
                restoredIndex = i;
                break;
            }
        }
        m_pomodoroTaskSelector->setCurrentIndex(restoredIndex);
        m_pomodoroTaskSelector->blockSignals(false);
    }

    refreshPomodoroTaskTimingPanel();

    if (m_focusHistoryList != nullptr) {
        m_focusHistoryList->clear();

        struct FocusHistoryEntry {
            QDateTime completedAt;
            int focusMinutes = 0;
            QString taskTitle;
        };

        QList<FocusHistoryEntry> entries;
        const QList<TodoItem> allTodos = m_storage.allTodos();
        for (const TodoItem &todo : allTodos) {
            for (const TodoItem::FocusRecord &record : todo.focusRecords) {
                if (record.completedAt.date() != QDate::currentDate()) {
                    continue;
                }

                FocusHistoryEntry entry;
                entry.completedAt = record.completedAt;
                entry.focusMinutes = record.focusMinutes;
                entry.taskTitle = todo.title;
                entries.push_back(entry);
            }
        }

        std::sort(entries.begin(), entries.end(), [](const FocusHistoryEntry &lhs, const FocusHistoryEntry &rhs) {
            return lhs.completedAt > rhs.completedAt;
        });

        for (const FocusHistoryEntry &entry : entries) {
            QString line = entry.completedAt.toString("HH:mm");
            if (entry.focusMinutes > 0) {
                line += QString(" · %1 分钟").arg(entry.focusMinutes);
            }
            line += QString(" · %1").arg(entry.taskTitle);
            m_focusHistoryList->addItem(line);
        }

        if (m_focusHistoryList->count() == 0) {
            m_focusHistoryList->addItem("今天还没有完成专注周期");
        }
    }
}

void MainWindow::refreshTagPresets() {
    if (m_tagEditor != nullptr) {
        m_tagEditor->setAvailableTags(m_storage.availableTags());
    }
    if (m_tagFilterInput != nullptr) {
        m_tagFilterInput->setAvailableTags(m_storage.availableTags());
    }
}

void MainWindow::refreshPomodoroTaskTimingPanel() {
    if (m_pomodoroTaskSelector == nullptr || m_pomodoroTaskTimingLabel == nullptr
        || m_pomodoroTaskTimingButton == nullptr) {
        return;
    }

    const QString selectedTaskId = m_pomodoroTaskSelector->currentData().toString();
    const auto selectedTodo = selectedTaskId.isEmpty() ? std::optional<TodoItem>() : m_storage.todoById(selectedTaskId);
    const auto activeTodo = m_storage.activeTimedTodo();
    const bool pomodoroRunning = m_timer.isRunning();
    const bool taskSelectionLocked = m_pomodoroTaskSelectionLocked || activeTodo.has_value();

    m_pomodoroTaskSelector->setEnabled(!taskSelectionLocked);

    if (!selectedTodo.has_value()) {
        m_pomodoroTaskTimingLabel->setText("累计时长 00:00:00");
        m_pomodoroTaskTimingButton->setText("开始任务计时");
        m_pomodoroTaskTimingButton->setObjectName("secondaryButton");
        m_pomodoroTaskTimingButton->setEnabled(false);
        style()->unpolish(m_pomodoroTaskTimingButton);
        style()->polish(m_pomodoroTaskTimingButton);
        return;
    }

    const bool isSelectedTiming = selectedTodo->timingStartedAt.isValid();
    const qint64 trackedSeconds = TaskStorage::effectiveTrackedSeconds(*selectedTodo, QDateTime::currentDateTime());
    m_pomodoroTaskTimingLabel->setText(QString("累计时长 %1").arg(formatDuration(trackedSeconds)));

    if (isSelectedTiming) {
        m_pomodoroTaskTimingButton->setText("结束任务计时");
        m_pomodoroTaskTimingButton->setObjectName("primaryButton");
        m_pomodoroTaskTimingButton->setEnabled(true);
    } else if (m_pomodoroTaskSelectionLocked) {
        m_pomodoroTaskTimingButton->setText("开始任务计时");
        m_pomodoroTaskTimingButton->setObjectName("secondaryButton");
        m_pomodoroTaskTimingButton->setEnabled(false);
    } else if (activeTodo.has_value() && activeTodo->id != selectedTodo->id) {
        m_pomodoroTaskTimingButton->setText("开始任务计时");
        m_pomodoroTaskTimingButton->setObjectName("secondaryButton");
        m_pomodoroTaskTimingButton->setEnabled(false);
    } else if (selectedTodo->completed) {
        m_pomodoroTaskTimingButton->setText("开始任务计时");
        m_pomodoroTaskTimingButton->setObjectName("secondaryButton");
        m_pomodoroTaskTimingButton->setEnabled(false);
    } else {
        m_pomodoroTaskTimingButton->setText("开始任务计时");
        m_pomodoroTaskTimingButton->setObjectName("secondaryButton");
        m_pomodoroTaskTimingButton->setEnabled(true);
    }

    m_startPauseButton->setEnabled(!activeTodo.has_value());
    m_stopPomodoroButton->setEnabled(!activeTodo.has_value() && m_pomodoroTaskSelectionLocked);
    m_resetButton->setEnabled(!activeTodo.has_value());
    m_focusMinutes->setEnabled(!m_pomodoroTaskSelectionLocked && !activeTodo.has_value());
    m_breakMinutes->setEnabled(!m_pomodoroTaskSelectionLocked && !activeTodo.has_value());
    m_presetBalancedButton->setEnabled(!m_pomodoroTaskSelectionLocked && !activeTodo.has_value());
    m_presetDeepFocusButton->setEnabled(!m_pomodoroTaskSelectionLocked && !activeTodo.has_value());
    m_presetQuickButton->setEnabled(!m_pomodoroTaskSelectionLocked && !activeTodo.has_value());

    style()->unpolish(m_pomodoroTaskTimingButton);
    style()->polish(m_pomodoroTaskTimingButton);
}

QWidget *MainWindow::createTodoCard(const TodoItem &todo) {
    auto *card = new QFrame(m_todoList);
    card->setObjectName("todoCard");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    QString state = "active";
    if (isOverdueToday(todo)) {
        state = "overdue";
    } else if (todo.completed) {
        state = "done";
    }
    card->setProperty("todoState", state);
    card->setProperty("editingCard", todo.id == m_editingTodoId);

    auto *rootLayout = new QHBoxLayout(card);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);
    rootLayout->setAlignment(Qt::AlignTop);

    auto *checkBox = new QCheckBox(card);
    checkBox->setChecked(todo.completed);
    rootLayout->addWidget(checkBox, 0, Qt::AlignTop);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(6);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    auto *priorityBadge = new QLabel(priorityText(todo.priority), card);
    priorityBadge->setObjectName("todoPriorityBadge");
    switch (todo.priority) {
    case TaskPriority::High:
        priorityBadge->setProperty("priorityTone", "high");
        break;
    case TaskPriority::Low:
        priorityBadge->setProperty("priorityTone", "low");
        break;
    case TaskPriority::Medium:
    default:
        priorityBadge->setProperty("priorityTone", "medium");
        break;
    }

    auto *titleLabel = new QLabel(todo.title, card);
    titleLabel->setObjectName("todoTitle");
    titleLabel->setProperty("completed", todo.completed);
    titleLabel->setWordWrap(true);
    QFont titleFont = titleLabel->font();
    titleFont.setStrikeOut(todo.completed);
    titleFont.setItalic(todo.completed);
    titleLabel->setFont(titleFont);

    auto *stateBadge = new QLabel(card);
    stateBadge->setObjectName("todoStateBadge");
    if (todo.completed) {
        stateBadge->setText("已完成");
        stateBadge->setProperty("badgeTone", "done");
    } else if (isOverdueToday(todo)) {
        stateBadge->setText("已逾期");
        stateBadge->setProperty("badgeTone", "overdue");
    } else {
        stateBadge->setText("进行中");
        stateBadge->setProperty("badgeTone", "active");
    }

    titleRow->addWidget(priorityBadge, 0, Qt::AlignTop);
    titleRow->addWidget(titleLabel, 1, Qt::AlignTop);
    contentLayout->addLayout(titleRow);

    auto makeChip = [card](const QString &text, const QString &tone) {
        auto *chip = new QLabel(text, card);
        chip->setObjectName("todoMetaChip");
        chip->setProperty("chipTone", tone);
        return chip;
    };

    auto *metaRow = new QHBoxLayout();
    metaRow->setContentsMargins(0, 0, 0, 0);
    metaRow->setSpacing(6);
    if (!todo.sourcePlanId.isEmpty()) {
        metaRow->addWidget(makeChip("计划任务", "tag"));
    }
    metaRow->addWidget(makeChip(QString("任务日 %1").arg(todo.date.toString("yyyy-MM-dd")), "neutral"));

    if (todo.dueAt.isValid()) {
        metaRow->addWidget(makeChip(QString("截止 %1").arg(todo.dueAt.toString("MM-dd HH:mm")),
                                    isOverdueToday(todo) ? "warn" : "neutral"));
    }

    if (todo.completed && todo.completedAt.isValid()) {
        metaRow->addWidget(makeChip(QString("完成 %1").arg(todo.completedAt.toString("MM-dd HH:mm")), "done"));
    }

    metaRow->addStretch(1);
    contentLayout->addLayout(metaRow);

    if (!todo.tags.isEmpty()) {
        auto *tagPanel = new QWidget(card);
        tagPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto *tagRow = new FlowLayout(tagPanel, 0, 6, 6);
        for (const QString &tag : todo.tags) {
            tagRow->addWidget(makeChip(tag, "tag"));
        }
        contentLayout->addWidget(tagPanel);
    }

    rootLayout->addLayout(contentLayout, 1);

    if (todo.completed) {
        stateBadge->setToolTip("任务状态：已完成");
    } else if (isOverdueToday(todo)) {
        stateBadge->setToolTip("任务状态：已逾期");
    } else {
        stateBadge->setToolTip("任务状态：进行中");
    }

    auto *actionWrap = new QWidget(card);
    auto *actionLayout = new QHBoxLayout(actionWrap);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);
    actionLayout->setAlignment(Qt::AlignVCenter);
    actionLayout->addWidget(stateBadge, 0, Qt::AlignVCenter);

    auto *deleteButton = new QPushButton(card);
    deleteButton->setObjectName("cardIconButton");
    deleteButton->setIcon(QIcon(":/icons/delete-task.png"));
    deleteButton->setProperty("normalIcon", ":/icons/delete-task.png");
    deleteButton->setProperty("hoverIcon", ":/icons/delete-task-hover.png");
    deleteButton->setIconSize(QSize(32, 32));
    deleteButton->setFixedSize(36, 36);
    deleteButton->setToolTip("删除任务");
    actionLayout->addWidget(deleteButton, 0, Qt::AlignVCenter);
    rootLayout->addWidget(actionWrap, 0, Qt::AlignTop);

    connect(checkBox, &QCheckBox::toggled, this, [this, id = todo.id, checkBox](bool checked) {
        if (!m_storage.setTodoCompleted(id, checked)) {
            QSignalBlocker blocker(checkBox);
            checkBox->setChecked(!checked);
            showAppWarningDialog(this, "更新失败", "无法更新任务状态，请重试。");
            return;
        }

        refreshTodoList();
        refreshStats();
        updateTaskTimingRefreshState();
    });

    connect(deleteButton, &QPushButton::clicked, this, [this, id = todo.id]() {
        deleteTodoById(id);
    });

    installEventFilterOnWidgetTree(this, card);
    return card;
}

void MainWindow::refreshDateSidebar(const QList<TodoItem> &allTodos) {
    int total = 0;
    int completed = 0;
    for (const TodoItem &todo : allTodos) {
        if (!todoCoversDate(todo, m_selectedDate)) {
            continue;
        }

        ++total;
        if (todo.completed) {
            ++completed;
        }
    }

    const int focusCount = m_storage.focusCountForDate(m_selectedDate);
    m_selectedDateLabel->setText(formatDateTitle(m_selectedDate));
    m_selectedDateMetaLabel->setText(m_selectedDate == QDate::currentDate()
                                         ? "今天的任务与专注记录"
                                         : QString("浏览 %1 的任务安排").arg(m_selectedDate.toString("yyyy-MM-dd")));
    m_dayTaskCountLabel->setText(QString("任务总数\n%1 项").arg(total));
    m_dayDoneCountLabel->setText(QString("已完成\n%1 项").arg(completed));
    m_dayFocusCountLabel->setText(QString("番茄数\n%1 个").arg(focusCount));
    if (m_compactTaskCountLabel != nullptr) {
        m_compactTaskCountLabel->setText(QString("任务总数\n%1 项").arg(total));
    }
    if (m_compactDoneCountLabel != nullptr) {
        m_compactDoneCountLabel->setText(QString("已完成\n%1 项").arg(completed));
    }
    if (m_compactFocusCountLabel != nullptr) {
        m_compactFocusCountLabel->setText(QString("番茄数\n%1 个").arg(focusCount));
    }

    refreshCalendarHighlights(allTodos);
}

void MainWindow::refreshCalendarHighlights(const QList<TodoItem> &allTodos) {
    QTextCharFormat clearFormat;
    for (const QDate &date : m_calendarMarkedDates) {
        m_dateNavigator->setDateTextFormat(date, clearFormat);
    }

    QSet<QDate> allDates;
    QSet<QDate> activeDates;
    QSet<QDate> completedOnlyDates;
    QSet<QDate> overdueDates;

    for (const TodoItem &todo : allTodos) {
        if (!todo.date.isValid()) {
            continue;
        }

        const QDate endDate = todoDisplayEndDate(todo);
        for (QDate date = todo.date; date <= endDate; date = date.addDays(1)) {
            allDates.insert(date);
            if (todo.completed) {
                if (!activeDates.contains(date)) {
                    completedOnlyDates.insert(date);
                }
            } else {
                activeDates.insert(date);
                completedOnlyDates.remove(date);
                if (isOverdueToday(todo)) {
                    overdueDates.insert(date);
                }
            }
        }
    }

    for (const QDate &date : allDates) {
        QTextCharFormat format;
        format.setFontWeight(QFont::DemiBold);

        if (overdueDates.contains(date)) {
            format.setBackground(QColor("#fde0d7"));
            format.setForeground(QColor("#9b372f"));
        } else if (activeDates.contains(date)) {
            format.setBackground(QColor("#eadcc8"));
            format.setForeground(QColor("#6d5441"));
        } else if (completedOnlyDates.contains(date)) {
            format.setBackground(QColor("#dfe7da"));
            format.setForeground(QColor("#5a6f57"));
        }

        m_dateNavigator->setDateTextFormat(date, format);
    }

    m_calendarMarkedDates = allDates;
}

void MainWindow::applyTheme() {
    const QString baseStyle =
        QStringLiteral(R"(
        QMainWindow#HTodoWindow {
            background: #f5f5f5;
        }
        QWidget {
            background: transparent;
            color: #191919;
            font-size: 14px;
        }
        QFrame#surfaceCard {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 18px;
        }
        QFrame#sidebarCard {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 20px;
        }
        QFrame#surfaceCardSoft {
            background: #fafafa;
            border: 1px solid #e8e8e8;
            border-radius: 18px;
        }
        QFrame#floatingPanel {
            background: transparent;
            border: 0;
        }
        QFrame#floatingPanelSurface {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 20px;
        }
        QFrame#floatingPanelSurface QScrollArea {
            background: transparent;
            border: 0;
        }
        QFrame#floatingPanelSurface QLabel#sectionTitleSmall {
            color: #191919;
            font-size: 15px;
            font-weight: 700;
        }
        QScrollArea {
            background: transparent;
            border: 0;
        }
        QCalendarWidget QWidget {
            alternate-background-color: #ffffff;
            background: #ffffff;
        }
        QCalendarWidget QToolButton {
            color: #191919;
            font-weight: 700;
            border-radius: 12px;
            padding: 8px 12px;
            background: #f7f7f7;
            border: 1px solid #e6e6e6;
        }
        QCalendarWidget QToolButton#qt_calendar_monthbutton,
        QCalendarWidget QToolButton#qt_calendar_yearbutton {
            padding: 8px 24px 8px 12px;
        }
        QCalendarWidget QToolButton#qt_calendar_monthbutton::menu-indicator,
        QCalendarWidget QToolButton#qt_calendar_yearbutton::menu-indicator {
            subcontrol-origin: padding;
            subcontrol-position: center right;
            right: 8px;
        }
        QCalendarWidget QToolButton:hover {
            background: #ededed;
        }
        QMenu {
            background: #ffffff;
            color: #191919;
            border: 1px solid #dcdcdc;
            border-radius: 14px;
            padding: 6px;
        }
        QMenu::item {
            padding: 8px 12px;
            border-radius: 10px;
            margin: 1px 0;
        }
        QMenu::item:selected {
            background: #eaf8f0;
            color: #191919;
        }
        QCalendarWidget QMenu {
            background: #ffffff;
            color: #191919;
            border: 1px solid #dcdcdc;
            border-radius: 14px;
        }
        QCalendarWidget QSpinBox {
            margin: 2px;
            background: #ffffff;
            border: 1px solid #dcdcdc;
            border-radius: 8px;
            padding: 4px 8px;
        }
        QCalendarWidget QAbstractItemView:enabled,
        QCalendarWidget QTableView {
            color: #2f3437;
            background: #ffffff;
            alternate-background-color: #ffffff;
            selection-background-color: #07c160;
            selection-color: #ffffff;
            outline: 0;
            font-size: 12px;
        }
        QCalendarWidget QTableView QHeaderView::section {
            padding: 2px 0;
            border: 0;
            background: transparent;
            color: #5f6670;
            font-size: 11px;
            font-weight: 600;
        }
        QCalendarWidget QTableView::item {
            padding: 2px;
            border-radius: 8px;
        }
        QCalendarWidget QTableView::item:hover {
            background: #eaf8f0;
            color: #191919;
        }
        QCalendarWidget QTableView::item:selected:hover {
            background: #06ad56;
            color: #ffffff;
        }
        QTabWidget::pane {
            border: 1px solid #e6e6e6;
            border-radius: 20px;
            background: #fafafa;
            top: -1px;
        }
        QTabBar::tab {
            background: #ebebeb;
            color: #606266;
            padding: 11px 20px;
            margin-right: 10px;
            border-top-left-radius: 14px;
            border-top-right-radius: 14px;
            font-weight: 600;
        }
        QTabBar::tab:selected {
            background: #07c160;
            color: #ffffff;
        }
        QLineEdit, QComboBox, QDateEdit, QDateTimeEdit, QSpinBox, QListWidget {
            background: #ffffff;
            border: 1px solid #dcdcdc;
            border-radius: 14px;
            padding: 10px 12px;
            selection-background-color: #07c160;
            selection-color: #ffffff;
        }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QDateTimeEdit:focus, QSpinBox:focus {
            border: 1px solid #07c160;
        }
        QListWidget {
            padding: 0;
            outline: 0;
            background: transparent;
            border: 0;
        }
        QListWidget#focusHistoryList {
            background: transparent;
            border: 0;
            padding: 0;
        }
        QListWidget#focusHistoryList::item {
            background: #ffffff;
            border: 1px solid #ececec;
            border-radius: 12px;
            padding: 10px 12px;
            margin: 0 0 6px 0;
            color: #2f3437;
        }
        QListWidget#statsRankingList {
            background: transparent;
            border: 0;
            padding: 0;
        }
        QListWidget#statsRankingList::item {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 16px;
            padding: 12px 14px;
            margin: 0 0 8px 0;
            color: #2f3437;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 6px 0 6px 0;
        }
        QScrollBar::handle:vertical {
            background: #d7d7d7;
            border-radius: 5px;
            min-height: 48px;
        }
        QScrollBar::handle:vertical:hover {
            background: #c3c3c3;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
            background: transparent;
            border: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QComboBox, QDateEdit, QDateTimeEdit {
            padding-right: 28px;
        }
        QComboBox#pomodoroField, QSpinBox#pomodoroField {
            background: #ffffff;
            border: 1px solid #dcdcdc;
            border-radius: 12px;
            padding: 8px 12px;
        }
        QComboBox#pomodoroField:focus, QSpinBox#pomodoroField:focus {
            border: 1px solid #07c160;
        }
        QComboBox#filterCombo {
            background: transparent;
            border: 0;
            border-bottom: 1px solid #d6d6d6;
            border-radius: 0;
            padding: 6px 18px 6px 0;
            min-height: 24px;
        }
        QComboBox#filterCombo:focus {
            border: 0;
            border-bottom: 1px solid #07c160;
        }
        QComboBox#filterCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: center right;
            width: 18px;
            border: 0;
            background: transparent;
            border-radius: 0;
        }
        QComboBox::drop-down, QDateEdit::drop-down, QDateTimeEdit::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border: 0;
            border-left: 1px solid #ececec;
            background: #fafafa;
            border-top-right-radius: 14px;
            border-bottom-right-radius: 14px;
        }
        QComboBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
        }
        QComboBox::down-arrow:on {
            top: 0px;
            left: 0px;
        }
        QDateEdit::down-arrow, QDateTimeEdit::down-arrow, QComboBox::down-arrow {
            image: url(:/icons/chevron-down.svg);
            width: 12px;
            height: 12px;
        }
        QDateEdit::drop-down:hover, QDateTimeEdit::drop-down:hover {
            background: #f3f3f3;
        }
        QFrame#roundedComboPopup {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 14px;
        }
        QListWidget#roundedComboPopupList {
            background: transparent;
            color: #191919;
            border: 0;
            outline: 0;
            padding: 6px;
        }
        QListWidget#roundedComboPopupList::item {
            min-height: 28px;
            padding: 8px 12px;
            border-radius: 10px;
            margin: 0;
        }
        QListWidget#roundedComboPopupList::item:selected {
            background: #eaf8f0;
            color: #191919;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            width: 20px;
            border: 0;
            background: transparent;
        }
        QSpinBox::up-arrow, QSpinBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
        }
        QLineEdit:disabled, QComboBox:disabled, QDateEdit:disabled, QDateTimeEdit:disabled, QSpinBox:disabled {
            background: #f2f2f2;
            color: #999999;
            border: 1px solid #e4e4e4;
        }
        QListWidget::item {
            border: 0;
            padding: 0;
            margin: 0 0 2px 0;
        }
        QListWidget::item:selected {
            background: transparent;
            border: 0;
            color: #191919;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
        }
        QCheckBox::indicator:unchecked {
            border: 2px solid #b8b8b8;
            border-radius: 10px;
            background: #ffffff;
        }
        QCheckBox::indicator:unchecked:hover {
            border: 2px solid #07c160;
            background: #f3fcf7;
        }
        QCheckBox::indicator:checked {
            border: 2px solid #07c160;
            border-radius: 10px;
            background: #07c160;
        }
        QCheckBox::indicator:checked:hover {
            border: 2px solid #06ad56;
            background: #06ad56;
        }
        QFrame#todoCard {
            background: #ffffff;
            border: 1px solid #eaeaea;
            border-radius: 18px;
        }
        QFrame#todoCard:hover {
            background: #fcfcfc;
            border: 1px solid #dcdcdc;
        }
        QFrame#todoCard[todoState="done"] {
            background: #f7fbf8;
            border: 1px solid #d9f0e2;
        }
        QFrame#todoCard[todoState="done"]:hover {
            background: #f3faf5;
            border: 1px solid #cce8d7;
        }
        QFrame#todoCard[todoState="overdue"] {
            background: #fff7f7;
            border: 1px solid #ffd6d6;
        }
        QFrame#todoCard[todoState="overdue"]:hover {
            background: #fff2f2;
            border: 1px solid #ffc7c7;
        }
        QFrame#todoCard[editingCard="true"] {
            border: 2px solid #07c160;
        }
        QLabel#todoTitle {
            font-size: 15px;
            font-weight: 700;
            color: #191919;
        }
        QLabel#todoTitle[completed="true"] {
            color: #8c8c8c;
            text-decoration: line-through;
        }
        QLabel#todoPriorityBadge, QLabel#todoStateBadge, QLabel#todoMetaChip {
            border-radius: 10px;
            padding: 3px 8px;
            font-weight: 700;
        }
        QLabel#todoPriorityBadge[priorityTone="high"] {
            background: #ffe7e7;
            color: #fa5151;
        }
        QLabel#todoPriorityBadge[priorityTone="medium"] {
            background: #eaf8f0;
            color: #07c160;
        }
        QLabel#todoPriorityBadge[priorityTone="low"] {
            background: #f1f1f1;
            color: #666666;
        }
        QLabel#todoStateBadge[badgeTone="active"] {
            background: #eaf8f0;
            color: #07c160;
        }
        QLabel#todoStateBadge[badgeTone="done"] {
            background: #f0f9f4;
            color: #34c759;
        }
        QLabel#todoStateBadge[badgeTone="overdue"] {
            background: #ffe7e7;
            color: #fa5151;
        }
        QLabel#todoMetaChip {
            background: #f5f5f5;
            color: #666666;
        }
        QLabel#todoMetaChip[chipTone="warn"] {
            background: #ffe7e7;
            color: #fa5151;
        }
        QLabel#todoMetaChip[chipTone="active"] {
            background: #eaf8f0;
            color: #07c160;
        }
        QLabel#todoMetaChip[chipTone="done"] {
            background: #f0f9f4;
            color: #34c759;
        }
        QLabel#todoMetaChip[chipTone="tag"] {
            background: #ecf5ff;
            color: #576b95;
        }
)")
        + QStringLiteral(R"(
        QPushButton {
            background: #f2f2f2;
            border: 1px solid #e5e5e5;
            border-radius: 14px;
            padding: 10px 18px;
            color: #191919;
            font-weight: 600;
        }
        QPushButton:disabled {
            background: #f2f2f2;
            color: #a0a0a0;
            border: 1px solid #ebebeb;
        }
        QPushButton:hover {
            background: #ebebeb;
        }
        QPushButton#primaryButton {
            background: #07c160;
            color: #ffffff;
            border: 1px solid #07c160;
        }
        QPushButton#primaryButton:hover {
            background: #06ad56;
            border: 1px solid #06ad56;
        }
        QPushButton#secondaryButton {
            background: #f2f2f2;
            color: #191919;
            border: 1px solid #e5e5e5;
        }
        QPushButton#secondaryButton:hover {
            background: #ebebeb;
        }
        QFrame#floatingPanelSurface QPushButton#secondaryButton {
            border-radius: 16px;
            padding: 8px 14px;
            text-align: left;
        }
        QFrame#floatingPanelSurface QPushButton#secondaryButton[tagSelected="true"] {
            background: #eaf8f0;
            color: #07c160;
            border: 1px solid #bfe8cc;
        }
        QFrame#floatingPanelSurface QPushButton#secondaryButton[tagSelected="true"]:hover {
            background: #ddf4e6;
            border: 1px solid #a9ddb9;
        }
        QFrame#floatingTagChip {
            background: #fafafa;
            border: 1px solid #e8e8e8;
            border-radius: 16px;
        }
        QFrame#floatingTagChip[tagSelected="true"] {
            background: #eaf8f0;
            border: 1px solid #bfe8cc;
        }
        QFrame#floatingTagChip QPushButton#tagChipButton {
            background: transparent;
            border: 0;
            border-radius: 16px;
            padding: 8px 8px 8px 12px;
            text-align: left;
            color: #191919;
        }
        QFrame#floatingTagChip[tagSelected="true"] QPushButton#tagChipButton {
            color: #07c160;
        }
        QFrame#floatingTagChip QPushButton#tagChipButton:hover {
            background: transparent;
        }
        QFrame#floatingTagChip QPushButton#cardIconButton {
            min-width: 22px;
            max-width: 22px;
            min-height: 22px;
            max-height: 22px;
            margin-right: 6px;
        }
        QPushButton#cardIconButton {
            background: transparent;
            border: 0;
            padding: 0;
        }
        QPushButton#cardIconButton:hover {
            background: transparent;
            border: 0;
        }
        QToolTip {
            background: #fcfcfc;
            color: #50565f;
            border: 1px solid #f0f0f0;
            border-radius: 6px;
            padding: 2px 6px;
            font-size: 12px;
            font-weight: 400;
        }
        QLabel#pageTitle, QPushButton#pageTitle {
            font-size: 22px;
            font-weight: 700;
            color: #191919;
        }
        QPushButton#pageTitle {
            background: transparent;
            border: 0;
            padding: 0;
            text-align: center;
        }
        QPushButton#pageTitle:hover {
            color: #07c160;
        }
        QPushButton#headerIconButton {
            background: transparent;
            border: 0;
            padding: 0;
        }
        QPushButton#headerIconButton:hover {
            background: transparent;
            border: 0;
        }
        QPushButton#headerIconButton:pressed {
            background: transparent;
            border: 0;
        }
        QLabel#sidebarTitle {
            font-size: 20px;
            font-weight: 700;
            color: #191919;
        }
        QLabel#sectionTitle {
            font-size: 18px;
            font-weight: 700;
            color: #191919;
        }
        QLabel#sectionTitleSmall {
            font-size: 15px;
            font-weight: 700;
            color: #191919;
        }
        QFrame#emptyStateCard {
            background: #ffffff;
            border: 1px dashed #d9d9d9;
            border-radius: 18px;
        }
        QLabel#emptyStateTitle {
            font-size: 16px;
            font-weight: 700;
            color: #191919;
        }
        QLabel#emptyStateMeta {
            color: #7a7a7a;
        }
)")
        + QStringLiteral(R"(
        QLabel#overdueBanner {
            background: #fff1f0;
            color: #fa5151;
            border: 1px solid #ffd1cf;
            border-radius: 14px;
            padding: 10px 14px;
            font-weight: 700;
        }
        QLabel#timerDisplay {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 28px;
            color: #07c160;
            padding: 24px;
            font-weight: 700;
        }
        QLabel#phaseBadge {
            background: #eaf8f0;
            border-radius: 999px;
            padding: 8px 16px;
            color: #07c160;
            font-weight: 700;
        }
        QProgressBar#pomodoroProgress {
            background: #f1f1f1;
            border: 0;
            border-radius: 8px;
            min-height: 12px;
        }
        QProgressBar#pomodoroProgress::chunk {
            background: #07c160;
            border-radius: 8px;
        }
        QLabel#mutedText {
            color: #8c8c8c;
        }
        QFrame#statsMetricCard {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 18px;
            min-height: 132px;
        }
        QLabel#statsMetricTitle {
            color: #5f6670;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#statsMetricValue {
            color: #191919;
            font-size: 24px;
            font-weight: 700;
            padding: 0;
            margin: 0;
        }
        QLabel#statsHeroCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f3fbf6, stop:1 #eef6ff);
            border: 1px solid #d9ebe0;
            border-radius: 22px;
            padding: 22px 24px;
            font-size: 20px;
            font-weight: 700;
            color: #1f2d24;
            min-height: 118px;
        }
        QLabel#statsNarrativeCard {
            background: #ffffff;
            border: 1px solid #e8e8e8;
            border-radius: 18px;
            padding: 18px 20px;
            font-size: 15px;
            font-weight: 600;
            color: #30343a;
            min-height: 128px;
        }
        QLabel#sidebarStatCard {
            background: #f8f8f8;
            border: 1px solid #e8e8e8;
            border-radius: 16px;
            padding: 12px 14px;
            min-height: 52px;
            font-size: 14px;
            font-weight: 700;
            color: #191919;
        }
        QStatusBar {
            background: #fafafa;
            color: #8c8c8c;
            border-top: 1px solid #e8e8e8;
        }
    )");
    setStyleSheet(scaleStyleSheetPixels(baseStyle, m_uiScale));

    QTextCharFormat weekdayFormat;
    weekdayFormat.setForeground(QColor("#2f3437"));
    QTextCharFormat weekendFormat;
    weekendFormat.setForeground(QColor("#576b95"));
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#8c8c8c"));
    headerFormat.setFontWeight(QFont::DemiBold);

    m_dateNavigator->setHeaderTextFormat(headerFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Monday, weekdayFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Tuesday, weekdayFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Wednesday, weekdayFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Thursday, weekdayFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Friday, weekdayFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Saturday, weekendFormat);
    m_dateNavigator->setWeekdayTextFormat(Qt::Sunday, weekendFormat);
}

QString MainWindow::formatSeconds(int totalSeconds) {
    if (totalSeconds < 0) {
        totalSeconds = 0;
    }

    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString MainWindow::formatDuration(qint64 totalSeconds) {
    if (totalSeconds < 0) {
        totalSeconds = 0;
    }

    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString MainWindow::phaseText(PomodoroTimer::Phase phase) {
    return phase == PomodoroTimer::Phase::Focus ? "专注" : "休息";
}

void MainWindow::populateTodoForm(const TodoItem &todo) {
    m_editingTodoId = todo.id;
    m_taskDateInput->setDate(todo.date);
    m_dailyPlanEnabled->setChecked(false);
    m_planEndDateInput->setDate(todo.date);
    m_planEndDateInput->setEnabled(false);
    m_todoInput->setText(todo.title);

    for (int i = 0; i < m_priorityInput->count(); ++i) {
        if (m_priorityInput->itemData(i).toInt() == static_cast<int>(todo.priority)) {
            m_priorityInput->setCurrentIndex(i);
            break;
        }
    }

    m_dueAtEnabled->setChecked(todo.dueAt.isValid());
    m_dueAtInput->setDateTime(todo.dueAt.isValid() ? todo.dueAt : QDateTime::currentDateTime());
    if (m_tagEditor != nullptr) {
        m_tagEditor->setSelectedTags(todo.tags);
    }
    refreshTagPresets();
    updateTodoEditorState();
}

void MainWindow::resetTodoForm() {
    m_editingTodoId.clear();
    m_taskDateInput->setDate(m_selectedDate);
    m_dailyPlanEnabled->setChecked(false);
    m_planEndDateInput->setDate(m_selectedDate);
    m_planEndDateInput->setEnabled(false);
    m_todoInput->clear();
    m_priorityInput->setCurrentIndex(1);
    m_dueAtEnabled->setChecked(false);
    m_dueAtInput->setDateTime(QDateTime::currentDateTime());
    if (m_tagEditor != nullptr) {
        m_tagEditor->setSelectedTags({});
    }
    refreshTagPresets();
    if (m_todoList != nullptr) {
        m_todoList->clearSelection();
        m_todoList->setCurrentItem(nullptr);
    }
    updateTodoEditorState();
}

void MainWindow::refreshOverdueReminder(const QList<TodoItem> &todos) {
    int overdueCount = 0;
    for (const TodoItem &todo : todos) {
        if (isOverdueToday(todo)) {
            ++overdueCount;
        }
    }

    if (overdueCount > 0) {
        m_overdueReminderLabel->setText(QString("当前有 %1 个任务已逾期，请尽快处理。").arg(overdueCount));
        m_overdueReminderLabel->setVisible(true);
        if (overdueCount != m_lastOverdueCount) {
            statusBar()->showMessage(QString("提醒: 当前有 %1 个逾期任务").arg(overdueCount), 5000);
        }
    } else {
        m_overdueReminderLabel->clear();
        m_overdueReminderLabel->setVisible(false);
    }

    m_lastOverdueCount = overdueCount;
}

void MainWindow::updateTodoEditorState() {
    const bool editing = !m_editingTodoId.isEmpty();
    m_addTodoButton->setText(editing ? "保存修改" : "添加");
    m_cancelEditButton->setVisible(editing);
    m_editorCaptionLabel->setText(editing ? "编辑任务" : "新建任务");
    if (m_dailyPlanEnabled != nullptr) {
        m_dailyPlanEnabled->setEnabled(!editing);
    }
    if (m_planEndDateInput != nullptr) {
        m_planEndDateInput->setEnabled(!editing && m_dailyPlanEnabled != nullptr && m_dailyPlanEnabled->isChecked());
    }
    if (m_planHintLabel != nullptr) {
        m_planHintLabel->setText("保存后会在起止日期内每天生成 1 条任务。");
        m_planHintLabel->setVisible(!editing && m_dailyPlanEnabled != nullptr && m_dailyPlanEnabled->isChecked());
    }
}

bool MainWindow::matchesFilters(const TodoItem &todo) const {
    const int priorityFilter = m_priorityFilter->currentData().toInt();
    if (priorityFilter >= 0 && static_cast<int>(todo.priority) != priorityFilter) {
        return false;
    }

    const QStringList selectedFilterTags =
        m_tagFilterInput != nullptr ? m_tagFilterInput->selectedTags() : QStringList();
    if (selectedFilterTags.isEmpty()) {
        return true;
    }

    for (const QString &filterTag : selectedFilterTags) {
        if (filterTag.trimmed().isEmpty()) {
            continue;
        }
        for (const QString &tag : todo.tags) {
            if (QString::compare(tag, filterTag, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    return false;
}

bool MainWindow::isOverdueToday(const TodoItem &todo) const {
    return !todo.completed && todo.dueAt.isValid() && todo.dueAt < QDateTime::currentDateTime();
}

MainWindow::TodoListMode MainWindow::currentTodoListMode() const {
    return static_cast<TodoListMode>(m_viewModeFilter->currentData().toInt());
}

bool MainWindow::isMinimalMode() const {
    return true;
}

QStringList MainWindow::collectTags() const {
    return m_tagEditor != nullptr ? m_tagEditor->selectedTags() : QStringList();
}

QString MainWindow::priorityText(TaskPriority priority) {
    switch (priority) {
    case TaskPriority::High:
        return "高";
    case TaskPriority::Low:
        return "低";
    case TaskPriority::Medium:
    default:
        return "中";
    }
}

QString MainWindow::todoListModeText(TodoListMode mode) {
    switch (mode) {
    case TodoListMode::All:
        return "全部任务";
    case TodoListMode::Archive:
        return "已完成归档";
    case TodoListMode::Today:
    default:
        return "单日视图";
    }
}

QString MainWindow::windowLayoutModeText(WindowLayoutMode mode) {
    switch (mode) {
    case WindowLayoutMode::Compact:
        return "紧凑布局";
    case WindowLayoutMode::Standard:
    default:
        return "紧凑布局";
    }
}

QString MainWindow::formatDateTitle(const QDate &date) {
    if (date == QDate::currentDate()) {
        return QString("今天 · %1").arg(date.toString("yyyy-MM-dd ddd"));
    }

    return date.toString("yyyy-MM-dd ddd");
}

QString MainWindow::formatTodoText(const TodoItem &todo) {
    QStringList metaParts;
    if (!todo.completed && todo.dueAt.isValid() && todo.dueAt < QDateTime::currentDateTime()) {
        metaParts.push_back("已逾期");
    }

    if (todo.date != QDate::currentDate()) {
        metaParts.push_back(QString("任务日: %1").arg(todo.date.toString("yyyy-MM-dd")));
    }

    if (todo.dueAt.isValid()) {
        metaParts.push_back(QString("截止: %1").arg(todo.dueAt.toString("yyyy-MM-dd HH:mm")));
    }

    if (todo.completed && todo.completedAt.isValid()) {
        metaParts.push_back(QString("完成: %1").arg(todo.completedAt.toString("yyyy-MM-dd HH:mm")));
    }

    metaParts.push_back(QString("计时: %1")
                            .arg(formatDuration(TaskStorage::effectiveTrackedSeconds(todo, QDateTime::currentDateTime()))));

    if (!todo.tags.isEmpty()) {
        metaParts.push_back(QString("标签: %1").arg(todo.tags.join(", ")));
    }

    const QString titleLine = QString("[%1] %2").arg(priorityText(todo.priority), todo.title);
    if (metaParts.isEmpty()) {
        return titleLine;
    }

    return QString("%1\n%2").arg(titleLine, metaParts.join(" | "));
}
