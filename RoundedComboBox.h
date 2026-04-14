#pragma once

#include <QComboBox>

class QFrame;
class QListWidget;
class QListWidgetItem;

class RoundedComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit RoundedComboBox(QWidget *parent = nullptr);

protected:
    void showPopup() override;
    void hidePopup() override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void ensurePopup();
    void syncPopupItems();
    void positionPopup();
    void applySelection(QListWidgetItem *item);

    QFrame *m_popup = nullptr;
    QListWidget *m_popupList = nullptr;
};
