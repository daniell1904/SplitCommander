#include "batchrenamer.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileInfo>

BatchRenamer::BatchRenamer(const QStringList &files, QWidget *parent)
    : QDialog(parent)
    , m_originalFiles(files) 
{
    Q_ASSERT(!files.isEmpty());

    setWindowTitle(tr("Batch Rename - %1 Dateien").arg(files.size()));
    resize(600, 450);
    setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
        .arg(TM().colors().bgBox, TM().colors().textPrimary));

    auto *layout = new QVBoxLayout(this);
    Q_ASSERT(layout != nullptr);

    auto *formLayout = new QGridLayout();
    Q_ASSERT(formLayout != nullptr);
    
    m_searchEdit = new QLineEdit();
    Q_ASSERT(m_searchEdit != nullptr);
    m_searchEdit->setPlaceholderText(QStringLiteral("Suchen..."));
    
    m_replaceEdit = new QLineEdit();
    Q_ASSERT(m_replaceEdit != nullptr);
    m_replaceEdit->setPlaceholderText(QStringLiteral("Ersetzen durch..."));
    
    m_prefixEdit = new QLineEdit();
    Q_ASSERT(m_prefixEdit != nullptr);
    m_prefixEdit->setPlaceholderText(QStringLiteral("Präfix hinzufügen..."));
    
    m_suffixEdit = new QLineEdit();
    Q_ASSERT(m_suffixEdit != nullptr);
    m_suffixEdit->setPlaceholderText(QStringLiteral("Suffix hinzufügen..."));

    const QString editStyle = QStringLiteral("QLineEdit { background: %1; border: 1px solid %2; padding: 5px; color: %3; }")
        .arg(TM().colors().bgList, TM().colors().borderAlt, TM().colors().textAccent);
    m_searchEdit->setStyleSheet(editStyle);
    m_replaceEdit->setStyleSheet(editStyle);
    m_prefixEdit->setStyleSheet(editStyle);
    m_suffixEdit->setStyleSheet(editStyle);

    formLayout->addWidget(new QLabel(QStringLiteral("Suchen:")), 0, 0);
    formLayout->addWidget(m_searchEdit, 0, 1);
    formLayout->addWidget(new QLabel(QStringLiteral("Ersetzen:")), 1, 0);
    formLayout->addWidget(m_replaceEdit, 1, 1);
    formLayout->addWidget(new QLabel(QStringLiteral("Präfix:")), 2, 0);
    formLayout->addWidget(m_prefixEdit, 2, 1);
    formLayout->addWidget(new QLabel(QStringLiteral("Suffix:")), 3, 0);
    formLayout->addWidget(m_suffixEdit, 3, 1);

    layout->addLayout(formLayout);

    layout->addWidget(new QLabel(QStringLiteral("VORSCHAU:")));
    m_previewList = new QListWidget();
    Q_ASSERT(m_previewList != nullptr);
    m_previewList->setStyleSheet(QStringLiteral("background: %1; border: none; font-family: monospace;")
        .arg(TM().colors().bgInput));
    layout->addWidget(m_previewList);

    auto *btnLayout = new QHBoxLayout();
    Q_ASSERT(btnLayout != nullptr);
    auto *okBtn = new QPushButton(QStringLiteral("Umbenennen"));
    Q_ASSERT(okBtn != nullptr);
    auto *cancelBtn = new QPushButton(QStringLiteral("Abbrechen"));
    Q_ASSERT(cancelBtn != nullptr);
    okBtn->setStyleSheet(QStringLiteral("background: %1; padding: 8px; font-weight: bold;").arg(TM().colors().accent));
    cancelBtn->setStyleSheet(QStringLiteral("background: %1; padding: 8px;").arg(TM().colors().borderAlt));
    
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

void BatchRenamer::updatePreview()
{
    Q_ASSERT(m_previewList != nullptr);
    Q_ASSERT(m_searchEdit != nullptr);

    m_previewList->clear();
    for (const QString &path : m_originalFiles)
    {
        const QFileInfo fi(path);
        QString name = fi.baseName();
        const QString ext = fi.completeSuffix();

        if (!m_searchEdit->text().isEmpty())
        {
            name.replace(m_searchEdit->text(), m_replaceEdit->text());
        }

        QString newName = m_prefixEdit->text() + name + m_suffixEdit->text();
        if (!ext.isEmpty())
        {
            newName += QLatin1Char('.') + ext;
        }

        m_previewList->addItem(fi.fileName() + QStringLiteral("  ->  ") + newName);
    }
}

QStringList BatchRenamer::newNames() const
{
    Q_ASSERT(m_previewList != nullptr);
    Q_ASSERT(m_previewList->count() == m_originalFiles.size());

    QStringList result;
    for (int i = 0; i < m_previewList->count(); ++i)
    {
        auto *item = m_previewList->item(i);
        Q_ASSERT(item != nullptr);
        const QString text = item->text();
        result << text.split(QStringLiteral("  ->  ")).last();
    }
    return result;
}