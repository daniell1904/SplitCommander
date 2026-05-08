#pragma once
#include <QDialog>
#include "settingsdialog.h"
#include <QList>
#include <QKeySequenceEdit>

/**
 * @brief Dialog zur Anpassung und Verwaltung von Tastenkürzeln (Shortcuts).
 */
class ShortcutDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutDialog(QWidget *parent = nullptr);

    static QString shortcut(const QString &id);
    static QList<ShortcutEntry> allShortcuts();

signals:
    void shortcutsChanged();

private:
    void buildUi();
    void save();
    void resetAll();

    QList<QKeySequenceEdit *> m_edits;
};
