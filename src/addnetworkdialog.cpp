#include "addnetworkdialog.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QIcon>
#include <QPixmap>
#include <QUrl>

AddNetworkDialog::AddNetworkDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SMB Laufwerke verbinden"));
    setMinimumWidth(420);
    setStyleSheet(TM().ssDialog());

    auto *mainLay = new QVBoxLayout(this);
    Q_ASSERT(mainLay != nullptr);
    mainLay->setSpacing(12);
    mainLay->setContentsMargins(16, 16, 16, 16);

    buildUI(mainLay);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    Q_ASSERT(buttons != nullptr);
    buttons->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; border:1px solid %2; color:%3; "
        "padding:5px 16px; border-radius:3px; font-size:11px; min-width:70px; }"
        "QPushButton:hover { background:%4; }"
        "QPushButton:default { border-color:%5; color:%5; }")
        .arg(TM().colors().bgBox, TM().colors().borderAlt,
             TM().colors().textPrimary, TM().colors().bgHover,
             TM().colors().accent));

    setupConnections(buttons);
    mainLay->addWidget(buttons);
}

void AddNetworkDialog::buildUI(QVBoxLayout *mainLay)
{
    auto *form = new QFormLayout();
    Q_ASSERT(form != nullptr);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    const QString inputStyle = QStringLiteral(
        "QLineEdit { background:%1; border:1px solid %2; color:%3; "
        "padding:4px 8px; border-radius:3px; font-size:12px; }"
        "QLineEdit:focus { border-color:%4; }")
        .arg(TM().colors().bgInput, TM().colors().borderAlt,
             TM().colors().textPrimary, TM().colors().accent);

    buildInputFields(form, inputStyle);
    buildIconSelector(form);

    mainLay->addLayout(form);
}

void AddNetworkDialog::buildInputFields(QFormLayout *form, const QString &inputStyle)
{
    const QString labelStyle = QStringLiteral("color:%1; font-size:11px;").arg(TM().colors().textMuted);

    m_urlEdit = new QLineEdit(this);
    Q_ASSERT(m_urlEdit != nullptr);
    m_urlEdit->setPlaceholderText(QStringLiteral("smb://192.168.0.1/Freigabe"));
    m_urlEdit->setStyleSheet(inputStyle);

    m_nameEdit = new QLineEdit(this);
    Q_ASSERT(m_nameEdit != nullptr);
    m_nameEdit->setPlaceholderText(tr("Anzeigename"));
    m_nameEdit->setStyleSheet(inputStyle);

    auto *urlLabel  = new QLabel(tr("Adresse:"),  this);
    Q_ASSERT(urlLabel != nullptr);
    auto *nameLabel = new QLabel(tr("Name:"),      this);
    Q_ASSERT(nameLabel != nullptr);

    urlLabel->setStyleSheet(labelStyle);
    nameLabel->setStyleSheet(labelStyle);

    form->addRow(urlLabel,  m_urlEdit);
    form->addRow(nameLabel, m_nameEdit);
}

void AddNetworkDialog::buildIconSelector(QFormLayout *form)
{
    const QString labelStyle = QStringLiteral("color:%1; font-size:11px;").arg(TM().colors().textMuted);

    m_iconCombo = new QComboBox(this);
    Q_ASSERT(m_iconCombo != nullptr);
    m_iconCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:1px solid %2; color:%3; "
        "padding:4px 8px; border-radius:3px; font-size:11px; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox QAbstractItemView { background:%1; color:%3; "
        "selection-background-color:%4; border:1px solid %2; }")
        .arg(TM().colors().bgInput, TM().colors().borderAlt,
             TM().colors().textPrimary, TM().colors().bgSelect));

    const QList<QPair<QString, QString>> icons = {
        {QStringLiteral("folder-remote-smb"),  tr("Freigegebener Ordner (SMB)")},
        {QStringLiteral("network-connect"),    tr("SSH / SFTP")},
        {QStringLiteral("folder-gdrive"),      tr("Google Drive")},
        {QStringLiteral("network-server"),     tr("Netzwerkserver")},
        {QStringLiteral("multimedia-player"),  tr("MTP-Gerät")},
        {QStringLiteral("bluetooth"),          tr("Bluetooth")},
        {QStringLiteral("folder-network"),     tr("Netzwerkordner")},
    };
    for (const auto &p : icons)
    {
        m_iconCombo->addItem(QIcon::fromTheme(p.first), p.second, p.first);
    }

    m_iconPreview = new QLabel(this);
    Q_ASSERT(m_iconPreview != nullptr);
    m_iconPreview->setFixedSize(32, 32);
    m_iconPreview->setAlignment(Qt::AlignCenter);

    auto *iconRow = new QHBoxLayout();
    Q_ASSERT(iconRow != nullptr);
    iconRow->setSpacing(8);
    iconRow->addWidget(m_iconCombo, 1);
    iconRow->addWidget(m_iconPreview);

    auto *iconLabel = new QLabel(tr("Symbol:"),    this);
    Q_ASSERT(iconLabel != nullptr);
    iconLabel->setStyleSheet(labelStyle);

    form->addRow(iconLabel, iconRow);
}

void AddNetworkDialog::setupConnections(QDialogButtonBox *buttons)
{
    connect(m_urlEdit, &QLineEdit::textChanged, this, &AddNetworkDialog::autoDeriveNameAndIcon);

    connect(m_iconCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddNetworkDialog::updateIcon);
    updateIcon();

    connect(buttons, &QDialogButtonBox::accepted, this, [this]()
    {
        Q_ASSERT(m_urlEdit != nullptr);
        if (!m_urlEdit->text().trimmed().isEmpty())
        {
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AddNetworkDialog::autoDeriveNameAndIcon(const QString &text)
{
    Q_ASSERT(m_nameEdit != nullptr);
    Q_ASSERT(m_iconCombo != nullptr);
    if (!m_nameEdit->isModified())
    {
        const QUrl urlObj = QUrl::fromUserInput(text);
        const QString scheme = urlObj.scheme().toLower();
        QString derived;
        if (scheme == QStringLiteral("gdrive"))
        {
            derived = urlObj.path().section(QLatin1Char('/'), 1, 1);
        }
        if (derived.isEmpty())
        {
            derived = urlObj.fileName();
        }
        if (derived.isEmpty() && !urlObj.host().isEmpty())
        {
            derived = urlObj.host();
        }
        m_nameEdit->setText(derived);
        m_nameEdit->setModified(false);

        int idx = 0;
        if (scheme == QStringLiteral("sftp") || scheme == QStringLiteral("ssh"))
        {
            idx = 1;
        }
        else if (scheme == QStringLiteral("gdrive"))
        {
            idx = 2;
        }
        else if (scheme == QStringLiteral("mtp"))
        {
            idx = 4;
        }
        else if (scheme == QStringLiteral("bluetooth"))
        {
            idx = 5;
        }
        m_iconCombo->setCurrentIndex(idx);
    }
    updateIcon();
}

QString AddNetworkDialog::url() const
{
    Q_ASSERT(m_urlEdit != nullptr);
    QString raw = m_urlEdit->text().trimmed();
    if (raw.isEmpty())
    {
        return raw;
    }
    if (!raw.contains(QStringLiteral("://")))
    {
        raw = QStringLiteral("smb://") + raw;
    }
    QUrl u(raw);
    if (u.path().isEmpty())
    {
        u.setPath(QStringLiteral("/"));
    }
    return u.toString();
}

QString AddNetworkDialog::name() const
{
    Q_ASSERT(m_nameEdit != nullptr);
    const QString n = m_nameEdit->text().trimmed();
    return n.isEmpty() ? url() : n;
}

QString AddNetworkDialog::iconName() const
{
    Q_ASSERT(m_iconCombo != nullptr);
    return m_iconCombo->currentData().toString();
}

void AddNetworkDialog::updateIcon()
{
    Q_ASSERT(m_iconCombo != nullptr);
    Q_ASSERT(m_iconPreview != nullptr);
    const QString icon = m_iconCombo->currentData().toString();
    m_iconPreview->setPixmap(QIcon::fromTheme(icon).pixmap(28, 28));
}
