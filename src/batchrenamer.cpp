#include "batchrenamer.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileInfo>

BatchRenamer::BatchRenamer(const QStringList &files, QWidget *parent)
    : QDialog(parent), m_originalFiles(files) 
{
    setWindowTitle(tr("Batch Rename - %1 Dateien").arg(files.size()));
    resize(600, 450);
    setStyleSheet(QString("background-color: %1; color: %2;")
        .arg(TM().colors().bgBox, TM().colors().textPrimary));

    auto *layout = new QVBoxLayout(this);

    // Eingabefelder
    auto *formLayout = new QGridLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Suchen...");
    m_replaceEdit = new QLineEdit();
    m_replaceEdit->setPlaceholderText("Ersetzen durch...");
    
    m_prefixEdit = new QLineEdit();
    m_prefixEdit->setPlaceholderText("Präfix hinzufügen...");
    m_suffixEdit = new QLineEdit();
    m_suffixEdit->setPlaceholderText("Suffix hinzufügen...");

    QString editStyle = QString("QLineEdit { background: %1; border: 1px solid %2; padding: 5px; color: %3; }")
        .arg(TM().colors().bgList, TM().colors().borderAlt, TM().colors().textAccent);
    m_searchEdit->setStyleSheet(editStyle);
    m_replaceEdit->setStyleSheet(editStyle);
    m_prefixEdit->setStyleSheet(editStyle);
    m_suffixEdit->setStyleSheet(editStyle);

    formLayout->addWidget(new QLabel("Suchen:"), 0, 0);
    formLayout->addWidget(m_searchEdit, 0, 1);
    formLayout->addWidget(new QLabel("Ersetzen:"), 1, 0);
    formLayout->addWidget(m_replaceEdit, 1, 1);
    formLayout->addWidget(new QLabel("Präfix:"), 2, 0);
    formLayout->addWidget(m_prefixEdit, 2, 1);
    formLayout->addWidget(new QLabel("Suffix:"), 3, 0);
    formLayout->addWidget(m_suffixEdit, 3, 1);

    layout->addLayout(formLayout);

    // Vorschau
    layout->addWidget(new QLabel("VORSCHAU:"));
    m_previewList = new QListWidget();
    m_previewList->setStyleSheet(QString("background: %1; border: none; font-family: monospace;")
        .arg(TM().colors().bgInput));
    layout->addWidget(m_previewList);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    auto *okBtn = new QPushButton("Umbenennen");
    auto *cancelBtn = new QPushButton("Abbrechen");
    okBtn->setStyleSheet(QString("background: %1; padding: 8px; font-weight: bold;").arg(TM().colors().accent));
    cancelBtn->setStyleSheet(QString("background: %1; padding: 8px;").arg(TM().colors().borderAlt));
    
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &BatchRenamer::updatePreview);
    connect(m_replaceEdit, &QLineEdit::textChanged, this, &BatchRenamer::updatePreview);
    connect(m_prefixEdit, &QLineEdit::textChanged, this, &BatchRenamer::updatePreview);
    connect(m_suffixEdit, &QLineEdit::textChanged, this, &BatchRenamer::updatePreview);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    updatePreview();
}

void BatchRenamer::updatePreview() {
    m_previewList->clear();
    for (const QString &path : m_originalFiles) {
        QFileInfo fi(path);
        QString name = fi.baseName();
        QString ext = fi.completeSuffix();

        if (!m_searchEdit->text().isEmpty()) {
            name.replace(m_searchEdit->text(), m_replaceEdit->text());
        }

        QString newName = m_prefixEdit->text() + name + m_suffixEdit->text();
        if (!ext.isEmpty()) newName += "." + ext;

        m_previewList->addItem(fi.fileName() + "  ->  " + newName);
    }
}

QStringList BatchRenamer::newNames() const {
    QStringList result;
    for (int i = 0; i < m_previewList->count(); ++i) {
        QString text = m_previewList->item(i)->text();
        result << text.split("  ->  ").last();
    }
    return result;
}