#include "addnetworkdialog.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QIcon>
#include <QPixmap>

AddNetworkDialog::AddNetworkDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SMB Laufwerke verbinden"));
    setMinimumWidth(420);
    setStyleSheet(TM().ssDialog());

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(12);
    mainLay->setContentsMargins(16, 16, 16, 16);

    auto *form = new QFormLayout();
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    const QString inputStyle = QString(
        "QLineEdit { background:%1; border:1px solid %2; color:%3; "
        "padding:4px 8px; border-radius:3px; font-size:12px; }"
        "QLineEdit:focus { border-color:%4; }")
        .arg(TM().colors().bgInput, TM().colors().borderAlt,
             TM().colors().textPrimary, TM().colors().accent);

    const QString labelStyle = QString("color:%1; font-size:11px;")
        .arg(TM().colors().textMuted);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("smb://192.168.0.1/Freigabe"));
    m_urlEdit->setStyleSheet(inputStyle);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Anzeigename"));
    m_nameEdit->setStyleSheet(inputStyle);

    // Symbol-Auswahl
    m_iconCombo = new QComboBox(this);
    m_iconCombo->setStyleSheet(QString(
        "QComboBox { background:%1; border:1px solid %2; color:%3; "
        "padding:4px 8px; border-radius:3px; font-size:11px; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox QAbstractItemView { background:%1; color:%3; "
        "selection-background-color:%4; border:1px solid %2; }")
        .arg(TM().colors().bgInput, TM().colors().borderAlt,
             TM().colors().textPrimary, TM().colors().bgSelect));

    const QList<QPair<QString,QString>> icons = {
        {QStringLiteral("folder-remote-smb"),  tr("Freigegebener Ordner (SMB)")},
        {QStringLiteral("network-connect"),    tr("SSH / SFTP")},
        {QStringLiteral("folder-gdrive"),      tr("Google Drive")},
        {QStringLiteral("network-server"),     tr("Netzwerkserver")},
        {QStringLiteral("multimedia-player"),  tr("MTP-Gerät")},
        {QStringLiteral("bluetooth"),          tr("Bluetooth")},
        {QStringLiteral("folder-network"),     tr("Netzwerkordner")},
    };
    for (const auto &p : icons)
        m_iconCombo->addItem(QIcon::fromTheme(p.first), p.second, p.first);

    m_iconPreview = new QLabel(this);
    m_iconPreview->setFixedSize(32, 32);
    m_iconPreview->setAlignment(Qt::AlignCenter);

    auto *iconRow = new QHBoxLayout();
    iconRow->setSpacing(8);
    iconRow->addWidget(m_iconCombo, 1);
    iconRow->addWidget(m_iconPreview);

    auto *urlLabel  = new QLabel(tr("Adresse:"),  this);
    auto *nameLabel = new QLabel(tr("Name:"),      this);
    auto *iconLabel = new QLabel(tr("Symbol:"),    this);
    for (auto *l : {urlLabel, nameLabel, iconLabel})
        l->setStyleSheet(labelStyle);

    form->addRow(urlLabel,  m_urlEdit);
    form->addRow(nameLabel, m_nameEdit);
    form->addRow(iconLabel, iconRow);
    mainLay->addLayout(form);

    // Auto-Name aus URL ableiten
    connect(m_urlEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (!m_nameEdit->isModified()) {
            const QUrl url = QUrl::fromUserInput(text);
            const QString scheme = url.scheme().toLower();
            QString derived;
            if (scheme == QStringLiteral("gdrive"))
                derived = url.path().section('/', 1, 1);
            if (derived.isEmpty())
                derived = url.fileName();
            if (derived.isEmpty() && !url.host().isEmpty())
                derived = url.host();
            m_nameEdit->setText(derived);
            m_nameEdit->setModified(false);

            // Icon auto-wählen
            int idx = 0;
            if (scheme == QStringLiteral("sftp") || scheme == QStringLiteral("ssh"))
                idx = 1;
            else if (scheme == QStringLiteral("gdrive"))
                idx = 2;
            else if (scheme == QStringLiteral("mtp"))
                idx = 4;
            else if (scheme == QStringLiteral("bluetooth"))
                idx = 5;
            m_iconCombo->setCurrentIndex(idx);
        }
        updateIcon();
    });

    connect(m_iconCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddNetworkDialog::updateIcon);
    updateIcon();

    // Buttons
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setStyleSheet(QString(
        "QPushButton { background:%1; border:1px solid %2; color:%3; "
        "padding:5px 16px; border-radius:3px; font-size:11px; min-width:70px; }"
        "QPushButton:hover { background:%4; }"
        "QPushButton:default { border-color:%5; color:%5; }")
        .arg(TM().colors().bgBox, TM().colors().borderAlt,
             TM().colors().textPrimary, TM().colors().bgHover,
             TM().colors().accent));

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (!m_urlEdit->text().trimmed().isEmpty())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLay->addWidget(buttons);
}

QString AddNetworkDialog::url() const {
    QString raw = m_urlEdit->text().trimmed();
    if (raw.isEmpty()) return raw;
    if (!raw.contains(QStringLiteral("://")))
        raw = QStringLiteral("smb://") + raw;
    QUrl u(raw);
    if (u.path().isEmpty())
        u.setPath(QStringLiteral("/"));
    return u.toString();
}

QString AddNetworkDialog::name() const {
    const QString n = m_nameEdit->text().trimmed();
    return n.isEmpty() ? url() : n;
}

QString AddNetworkDialog::iconName() const {
    return m_iconCombo->currentData().toString();
}

void AddNetworkDialog::updateIcon() {
    const QString icon = m_iconCombo->currentData().toString();
    m_iconPreview->setPixmap(QIcon::fromTheme(icon).pixmap(28, 28));
}
