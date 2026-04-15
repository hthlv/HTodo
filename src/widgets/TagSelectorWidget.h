#pragma once

#include <QFrame>
#include <QStringList>

class QGridLayout;
class QLineEdit;
class QScrollArea;
class QWidget;

class TagSelectorWidget : public QFrame {
    Q_OBJECT

public:
    explicit TagSelectorWidget(QWidget *parent = nullptr);

    void setAvailableTags(const QStringList &tags);
    void setSelectedTags(const QStringList &tags);
    QStringList selectedTags() const;
    void setPlaceholderText(const QString &text);

signals:
    void tagsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class FlowLayout;

    QStringList normalizedTags(const QStringList &tags) const;
    void promoteManualTags();
    void refreshSelectedTags();
    void refreshPopup();
    void positionPopup();
    void toggleTag(const QString &tag);
    void removeTag(const QString &tag);

    QStringList m_availableTags;
    QStringList m_selectedTags;

    QWidget *m_selectedPanel = nullptr;
    FlowLayout *m_selectedLayout = nullptr;
    QLineEdit *m_input = nullptr;

    QFrame *m_popup = nullptr;
    QScrollArea *m_popupScrollArea = nullptr;
    QWidget *m_popupContent = nullptr;
    QGridLayout *m_popupGrid = nullptr;
};
