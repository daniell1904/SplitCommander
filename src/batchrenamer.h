#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>

// --- BatchRenamer --- (Dialogfenster für die Massenumbenennung von Dateien)
class BatchRenamer : public QDialog {
    Q_OBJECT
public:
    explicit BatchRenamer(const QStringList &files, QWidget *parent = nullptr);
    QStringList newNames() const;

private slots:
    void updatePreview();

private:
    QStringList m_originalFiles;
    QLineEdit *m_searchEdit;
    QLineEdit *m_replaceEdit;
    QLineEdit *m_prefixEdit;
    QLineEdit *m_suffixEdit;
    QListWidget *m_previewList;
};

