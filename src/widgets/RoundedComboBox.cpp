#include "RoundedComboBox.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QWheelEvent>

RoundedComboBox::RoundedComboBox(QWidget *parent) : QComboBox(parent) {}

void RoundedComboBox::showPopup() {
    ensurePopup();
    syncPopupItems();
    positionPopup();
    m_popup->show();
    m_popup->raise();
}

void RoundedComboBox::hidePopup() {
    if (m_popup != nullptr) {
        m_popup->hide();
    }
}

bool RoundedComboBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_popupList && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (QListWidgetItem *item = m_popupList->currentItem(); item != nullptr) {
                applySelection(item);
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            hidePopup();
            return true;
        }
    }

    return QComboBox::eventFilter(watched, event);
}

void RoundedComboBox::wheelEvent(QWheelEvent *event) {
    event->ignore();
}

void RoundedComboBox::ensurePopup() {
    if (m_popup != nullptr) {
        return;
    }

    m_popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    m_popup->setObjectName("roundedComboPopup");
    m_popup->setFrameShape(QFrame::NoFrame);

    auto *layout = new QHBoxLayout(m_popup);
    layout->setContentsMargins(0, 0, 0, 0);

    m_popupList = new QListWidget(m_popup);
    m_popupList->setObjectName("roundedComboPopupList");
    m_popupList->setFrameShape(QFrame::NoFrame);
    m_popupList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popupList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_popupList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_popupList->setUniformItemSizes(true);
    m_popupList->setFocusPolicy(Qt::StrongFocus);
    m_popupList->installEventFilter(this);
    layout->addWidget(m_popupList);

    connect(m_popupList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        applySelection(item);
    });
}

void RoundedComboBox::syncPopupItems() {
    if (m_popupList == nullptr) {
        return;
    }

    m_popupList->clear();
    for (int i = 0; i < count(); ++i) {
        auto *item = new QListWidgetItem(itemText(i), m_popupList);
        item->setData(Qt::UserRole, itemData(i));
        item->setData(Qt::UserRole + 1, i);
        item->setToolTip(itemText(i));
        item->setSizeHint(QSize(0, 38));
    }

    if (currentIndex() >= 0 && currentIndex() < m_popupList->count()) {
        m_popupList->setCurrentRow(currentIndex());
    }
}

void RoundedComboBox::positionPopup() {
    if (m_popup == nullptr || m_popupList == nullptr) {
        return;
    }

    int widest = width();
    for (int i = 0; i < m_popupList->count(); ++i) {
        widest = qMax(widest, fontMetrics().horizontalAdvance(m_popupList->item(i)->text()) + 48);
    }

    const int visibleRows = qMin(qMax(m_popupList->count(), 1), 6);
    const int popupHeight = visibleRows * 38 + 12;

    m_popup->resize(widest, popupHeight);
    m_popupList->setFixedSize(m_popup->size());
    m_popupList->setVerticalScrollBarPolicy(m_popupList->count() > 6 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_popup->move(mapToGlobal(QPoint(0, height() + 6)));
}

void RoundedComboBox::applySelection(QListWidgetItem *item) {
    if (item == nullptr) {
        return;
    }

    const int index = item->data(Qt::UserRole + 1).toInt();
    if (index >= 0 && index < count()) {
        setCurrentIndex(index);
    }

    hidePopup();
}
