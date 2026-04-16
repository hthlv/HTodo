#include "TagSelectorWidget.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QVBoxLayout>

class TagSelectorWidget::FlowLayout : public QLayout {
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
        const QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
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

TagSelectorWidget::TagSelectorWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("surfaceCardSoft");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);

    m_selectedPanel = new QWidget(this);
    m_selectedPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_selectedLayout = new FlowLayout(m_selectedPanel, 0, 8, 8);

    m_input = new QLineEdit(this);
    m_input->setMinimumHeight(36);
    m_input->setMinimumWidth(220);
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_input->installEventFilter(this);

    m_selectedLayout->addWidget(m_input);
    layout->addWidget(m_selectedPanel);

    connect(m_input, &QLineEdit::returnPressed, this, &TagSelectorWidget::promoteManualTags);
    connect(m_input, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.contains(',') || text.contains(QChar(0xff0c))) {
            promoteManualTags();
        }
    });

    m_popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    m_popup->setObjectName("floatingPanel");
    m_popup->setAttribute(Qt::WA_TranslucentBackground, true);
    auto *popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(10, 10, 10, 10);
    popupLayout->setSpacing(0);

    auto *popupSurface = new QFrame(m_popup);
    popupSurface->setObjectName("floatingPanelSurface");
    auto *popupSurfaceLayout = new QVBoxLayout(popupSurface);
    popupSurfaceLayout->setContentsMargins(14, 14, 14, 14);
    popupSurfaceLayout->setSpacing(10);

    auto *popupTitle = new QLabel("点击标签添加或取消", popupSurface);
    popupTitle->setObjectName("sectionTitleSmall");

    m_popupScrollArea = new QScrollArea(popupSurface);
    m_popupScrollArea->setWidgetResizable(true);
    m_popupScrollArea->setFrameShape(QFrame::NoFrame);
    m_popupScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popupScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_popupScrollArea->setMaximumHeight(220);
    m_popupScrollArea->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_popupContent = new QWidget(m_popupScrollArea);
    m_popupContent->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_popupGrid = new QGridLayout(m_popupContent);
    m_popupGrid->setContentsMargins(0, 0, 20, 0);
    m_popupGrid->setHorizontalSpacing(8);
    m_popupGrid->setVerticalSpacing(8);
    m_popupScrollArea->setWidget(m_popupContent);

    popupSurfaceLayout->addWidget(popupTitle);
    popupSurfaceLayout->addWidget(m_popupScrollArea);
    popupLayout->addWidget(popupSurface);

    refreshSelectedTags();
    refreshPopup();
}

void TagSelectorWidget::setAvailableTags(const QStringList &tags) {
    m_availableTags = normalizedTags(tags);
    refreshPopup();
}

void TagSelectorWidget::setSelectedTags(const QStringList &tags) {
    m_selectedTags = normalizedTags(tags);
    m_input->clear();
    refreshSelectedTags();
    refreshPopup();
    emit tagsChanged();
}

QStringList TagSelectorWidget::selectedTags() const {
    return m_selectedTags;
}

void TagSelectorWidget::setPlaceholderText(const QString &text) {
    if (m_input != nullptr) {
        m_input->setPlaceholderText(text);
    }
}

void TagSelectorWidget::setManualEntryEnabled(bool enabled) {
    m_manualEntryEnabled = enabled;
    if (m_input != nullptr) {
        m_input->setReadOnly(!enabled);
        m_input->setCursor(enabled ? Qt::IBeamCursor : Qt::PointingHandCursor);
        if (!enabled) {
            m_input->clear();
        }
    }
}

bool TagSelectorWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_input && event->type() == QEvent::MouseButtonPress) {
        refreshPopup();
        positionPopup();
        m_popup->show();
        m_popup->raise();
    }

    return QFrame::eventFilter(watched, event);
}

QStringList TagSelectorWidget::normalizedTags(const QStringList &tags) const {
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
            normalized.append(tag);
        }
    }
    return normalized;
}

void TagSelectorWidget::refreshContainerGeometry() {
    updateGeometry();
    adjustSize();

    QWidget *ancestor = parentWidget();
    while (ancestor != nullptr) {
        ancestor->updateGeometry();
        ancestor->adjustSize();
        ancestor = ancestor->parentWidget();
    }
}

void TagSelectorWidget::promoteManualTags() {
    if (m_input == nullptr || !m_manualEntryEnabled) {
        return;
    }

    QStringList tags = m_selectedTags;
    const QStringList manualTags = m_input->text().split(QRegularExpression("\\s*[,，]\\s*"), Qt::SkipEmptyParts);
    bool changed = false;
    for (const QString &rawTag : manualTags) {
        const QString tag = rawTag.trimmed();
        if (tag.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const QString &existing : tags) {
            if (QString::compare(existing, tag, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            tags.append(tag);
            changed = true;
        }
    }

    if (!changed && m_input->text().trimmed().isEmpty()) {
        return;
    }

    m_selectedTags = normalizedTags(tags);
    m_input->clear();
    refreshSelectedTags();
    refreshPopup();
    emit tagsChanged();
}

void TagSelectorWidget::refreshSelectedTags() {
    if (m_selectedLayout == nullptr || m_selectedPanel == nullptr || m_input == nullptr) {
        return;
    }

    while (QLayoutItem *item = m_selectedLayout->takeAt(0)) {
        if (QWidget *widget = item->widget(); widget != nullptr) {
            if (widget != m_input) {
                delete widget;
            }
        }
        delete item;
    }

    for (const QString &tag : m_selectedTags) {
        auto *chip = new QFrame(m_selectedPanel);
        chip->setObjectName("surfaceCardSoft");
        auto *chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(10, 6, 8, 6);
        chipLayout->setSpacing(6);

        auto *chipLabel = new QLabel(tag, chip);
        chipLabel->setObjectName("sectionTitleSmall");
        chipLabel->setMaximumWidth(112);
        chipLabel->setToolTip(tag);
        chipLayout->addWidget(chipLabel);

        auto *removeButton = new QPushButton(chip);
        removeButton->setObjectName("cardIconButton");
        removeButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
        removeButton->setIconSize(QSize(12, 12));
        removeButton->setFixedSize(22, 22);
        chipLayout->addWidget(removeButton);

        connect(removeButton, &QPushButton::clicked, this, [this, tag]() {
            removeTag(tag);
        });

        m_selectedLayout->addWidget(chip);
    }

    m_selectedLayout->addWidget(m_input);
    m_selectedPanel->setVisible(true);
    m_selectedPanel->updateGeometry();
    m_selectedPanel->adjustSize();
    refreshContainerGeometry();
}

void TagSelectorWidget::refreshPopup() {
    if (m_popupGrid == nullptr || m_popupContent == nullptr || m_popup == nullptr || m_input == nullptr) {
        return;
    }

    while (QLayoutItem *item = m_popupGrid->takeAt(0)) {
        if (QWidget *widget = item->widget(); widget != nullptr) {
            widget->deleteLater();
        }
        delete item;
    }

    QList<QWidget *> popupChips;
    popupChips.reserve(m_availableTags.size());
    for (const QString &tag : m_availableTags) {
        auto *tagChip = new QFrame(m_popupContent);
        tagChip->setObjectName("floatingTagChip");
        tagChip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        bool selected = false;
        for (const QString &existing : m_selectedTags) {
            if (QString::compare(existing, tag, Qt::CaseInsensitive) == 0) {
                selected = true;
                break;
            }
        }
        tagChip->setProperty("tagSelected", selected);

        auto *tagRowLayout = new QHBoxLayout(tagChip);
        tagRowLayout->setContentsMargins(0, 0, 0, 0);
        tagRowLayout->setSpacing(0);

        auto *tagButton = new QPushButton(tag, tagChip);
        tagButton->setObjectName("tagChipButton");
        tagButton->setMinimumHeight(34);
        tagButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        tagButton->setMaximumWidth(108);
        tagButton->setToolTip(tag);
        connect(tagButton, &QPushButton::clicked, this, [this, tag]() {
            toggleTag(tag);
        });
        tagRowLayout->addWidget(tagButton);
        popupChips.push_back(tagChip);
    }

    constexpr int columnCount = 3;
    int maxChipWidth = 0;
    int maxChipHeight = 0;
    for (QWidget *chip : popupChips) {
        if (chip == nullptr) {
            continue;
        }
        chip->adjustSize();
        const QSize chipSize = chip->sizeHint();
        maxChipWidth = qMax(maxChipWidth, chipSize.width());
        maxChipHeight = qMax(maxChipHeight, chipSize.height());
    }

    for (int i = 0; i < popupChips.size(); ++i) {
        QWidget *chip = popupChips.at(i);
        if (chip == nullptr) {
            continue;
        }
        chip->setFixedWidth(maxChipWidth);
        m_popupGrid->addWidget(chip, i / columnCount, i % columnCount);
    }

    const int totalRows = qMax(1, (popupChips.size() + columnCount - 1) / columnCount);
    const int contentWidth = columnCount * maxChipWidth
                             + qMax(0, columnCount - 1) * m_popupGrid->horizontalSpacing()
                             + m_popupGrid->contentsMargins().left()
                             + m_popupGrid->contentsMargins().right();
    const int contentHeight = totalRows * maxChipHeight
                              + qMax(0, totalRows - 1) * m_popupGrid->verticalSpacing()
                              + m_popupGrid->contentsMargins().top()
                              + m_popupGrid->contentsMargins().bottom();

    m_popupContent->setFixedSize(contentWidth, contentHeight);
    m_popupContent->adjustSize();

    if (m_popupScrollArea != nullptr) {
        const int visibleRows = qMin(3, totalRows);
        const int targetHeight = qBound(40,
                                        visibleRows * maxChipHeight + qMax(0, visibleRows - 1) * m_popupGrid->verticalSpacing() + 2,
                                        contentHeight + 2);
        const bool needsScroll = contentHeight > targetHeight;
        m_popupScrollArea->setFixedHeight(targetHeight);
        m_popupScrollArea->setVerticalScrollBarPolicy(needsScroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);

        const int scrollBarReserve = needsScroll && m_popupScrollArea->verticalScrollBar() != nullptr
                                         ? m_popupScrollArea->verticalScrollBar()->sizeHint().width() + 8
                                         : 0;
        const int popupWidth = qMax(m_input->width(), contentWidth + scrollBarReserve + 16);
        const int currentPreferredWidth = m_popup->property("preferredWidth").toInt();
        const int stableWidth = m_popup->isVisible() ? qMax(currentPreferredWidth, popupWidth) : popupWidth;
        m_popup->setProperty("preferredWidth", stableWidth);
    }
}

void TagSelectorWidget::positionPopup() {
    if (m_popup == nullptr || m_input == nullptr) {
        return;
    }

    const int popupWidth = m_popup->property("preferredWidth").toInt() > 0
                               ? m_popup->property("preferredWidth").toInt()
                               : qMax(320, m_input->width());
    m_popup->setFixedWidth(popupWidth);
    m_popup->adjustSize();

    const QRect anchorRect(m_input->mapToGlobal(QPoint(0, 0)), m_input->size());
    QRect available = QRect(QPoint(0, 0), QSize(1280, 720));
    if (QScreen *screenHandle = m_input->screen(); screenHandle != nullptr) {
        available = screenHandle->availableGeometry();
    }

    const QSize popupSize = m_popup->size();
    int x = anchorRect.left();
    int y = anchorRect.bottom() + 8;

    if (x + popupSize.width() > available.right() - 12) {
        x = available.right() - popupSize.width() - 12;
    }
    if (x < available.left() + 12) {
        x = available.left() + 12;
    }
    if (y + popupSize.height() > available.bottom() - 12) {
        y = anchorRect.top() - popupSize.height() - 8;
    }
    if (y < available.top() + 12) {
        y = available.top() + 12;
    }

    m_popup->move(QPoint(x, y));
}

void TagSelectorWidget::toggleTag(const QString &tag) {
    QStringList tags = m_selectedTags;
    bool removed = false;
    for (int i = tags.size() - 1; i >= 0; --i) {
        if (QString::compare(tags[i], tag, Qt::CaseInsensitive) == 0) {
            tags.removeAt(i);
            removed = true;
        }
    }
    if (!removed) {
        tags.append(tag);
    }
    m_selectedTags = normalizedTags(tags);
    refreshSelectedTags();
    refreshPopup();
    emit tagsChanged();
}

void TagSelectorWidget::removeTag(const QString &tag) {
    QStringList tags;
    for (const QString &existing : m_selectedTags) {
        if (QString::compare(existing, tag, Qt::CaseInsensitive) != 0) {
            tags.append(existing);
        }
    }
    m_selectedTags = normalizedTags(tags);
    refreshSelectedTags();
    refreshPopup();
    emit tagsChanged();
}
