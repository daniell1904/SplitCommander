#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>

class BatchRenamer : public QDialog
{
    Q_OBJECT
public:
    explicit BatchRenamer(const QStringList &files, QWidget *parent = nullptr);
    [[nodiscard]] QStringList newNames() const;

private slots:
    void updatePreview();

private:
    QStringList m_originalFiles;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QLineEdit *m_prefixEdit = nullptr;
    QLineEdit *m_suffixEdit = nullptr;
    QListWidget *m_previewList = nullptr;

    void setupFormFields(class QGridLayout *formLay);
    void setupButtons(class QVBoxLayout *lay);
};
