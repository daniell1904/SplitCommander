#include "paperlessmanager.h"
#include "../../config.h"
#include "../../thememanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// ─── Auth-Header ──────────────────────────────────────────────────────────────
[[nodiscard]] static QNetworkRequest plRequest(const QString &endpoint)
{
    QNetworkRequest req(QUrl(Config::paperlessUrl() + endpoint));
    req.setRawHeader("Authorization", QByteArray("Token ") + Config::paperlessToken().toUtf8());
    req.setRawHeader("Accept", "application/json");
    if (Config::paperlessSslIgnore())
    {
        req.setRawHeader("ssl-verify", "false"); // handled via NAM sslErrors signal
    }
    return req;
}

static void ignoreSsl(QNetworkReply *reply)
{
    if (Config::paperlessSslIgnore())
    {
        QObject::connect(reply, &QNetworkReply::sslErrors, reply, qOverload<const QList<QSslError>&>(&QNetworkReply::ignoreSslErrors));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Upload-Dialog
// ═══════════════════════════════════════════════════════════════════════════════
PaperlessUploadDialog::PaperlessUploadDialog(const QStringList &files, QWidget *parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    const ThemeManager &tm = TM();
    setWindowTitle(tr("Zu Paperless hochladen"));
    setMinimumWidth(480);
    setStyleSheet(tm.ssDialog());

    const QString inputSS = QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:2px 8px; font-size:13px; min-height:22px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
    const QString labelSS = QString("color:%1; font-size:12px; font-weight:bold;")
        .arg(tm.colors().textPrimary);

    auto *lay = new QVBoxLayout(this);
    Q_ASSERT(lay != nullptr);
    lay->setSpacing(8);
    lay->setContentsMargins(16, 16, 16, 16);

    // Dateiliste
    auto *filesLbl = new QLabel(tr("Dateien:"), this);
    Q_ASSERT(filesLbl != nullptr);
    filesLbl->setStyleSheet(labelSS);
    lay->addWidget(filesLbl);
    auto *filesList = new QLabel(files.join(QStringLiteral("\n")), this);
    Q_ASSERT(filesList != nullptr);
    filesList->setStyleSheet(QString(
        "color:%1; font-size:11px; padding:4px; background:%2; "
        "border:1px solid %3; border-radius:3px;")
        .arg(tm.colors().textMuted, tm.colors().bgInput, tm.colors().borderAlt));
    filesList->setWordWrap(true);
    lay->addWidget(filesList);

    // Titel
    auto *titleLbl = new QLabel(tr("Titel (optional):"), this);
    Q_ASSERT(titleLbl != nullptr);
    titleLbl->setStyleSheet(labelSS);
    lay->addWidget(titleLbl);
    m_titleEdit = new QLineEdit(this);
    Q_ASSERT(m_titleEdit != nullptr);
    m_titleEdit->setStyleSheet(inputSS);
    if (files.size() == 1)
    {
        m_titleEdit->setText(QFileInfo(files.first()).completeBaseName());
    }
    else
    {
        m_titleEdit->setPlaceholderText(tr("Wird aus Dateiname ermittelt"));
    }
    lay->addWidget(m_titleEdit);

    // Tags
    auto *tagsLbl = new QLabel(tr("Tags:"), this);
    Q_ASSERT(tagsLbl != nullptr);
    tagsLbl->setStyleSheet(labelSS);
    lay->addWidget(tagsLbl);
    m_tagsList = new QListWidget(this);
    Q_ASSERT(m_tagsList != nullptr);
    m_tagsList->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt));
    m_tagsList->setMaximumHeight(120);
    m_tagsList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_tagsList->addItem(tr("Wird geladen…"));
    m_tagsList->setEnabled(false);
    lay->addWidget(m_tagsList);

    // Korrespondent
    auto *corrLbl = new QLabel(tr("Korrespondent:"), this);
    Q_ASSERT(corrLbl != nullptr);
    corrLbl->setStyleSheet(labelSS);
    lay->addWidget(corrLbl);
    m_corrCombo = new QComboBox(this);
    Q_ASSERT(m_corrCombo != nullptr);
    m_corrCombo->setStyleSheet(inputSS);
    m_corrCombo->addItem(tr("Wird geladen…"));
    m_corrCombo->setEnabled(false);
    lay->addWidget(m_corrCombo);

    // Buttons
    const QString primSS = QString(
        "QPushButton { border-radius:3px; padding:4px 16px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:none; }"
        "QPushButton:hover { background:%3; }")
        .arg(tm.colors().accent, tm.colors().textLight, tm.colors().accentHover);
    const QString normSS = QString(
        "QPushButton { border-radius:3px; padding:4px 16px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:1px solid %3; }"
        "QPushButton:hover { background:%4; }")
        .arg(tm.colors().bgPanel, tm.colors().textPrimary,
             tm.colors().borderAlt, tm.colors().bgHover);

    auto *btnRow = new QHBoxLayout();
    Q_ASSERT(btnRow != nullptr);
    auto *okBtn = new QPushButton(tr("Hochladen"), this);
    Q_ASSERT(okBtn != nullptr);
    okBtn->setIcon(QIcon::fromTheme(QStringLiteral("cloud-upload")));
    okBtn->setStyleSheet(primSS);
    auto *cancelBtn = new QPushButton(tr("Abbrechen"), this);
    Q_ASSERT(cancelBtn != nullptr);
    cancelBtn->setStyleSheet(normSS);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    connect(okBtn,     &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    loadMeta();
}

void PaperlessUploadDialog::loadMeta()
{
    // Tags
    auto *tagsReply = m_nam->get(plRequest(QStringLiteral("/api/tags/?page_size=100")));
    Q_ASSERT(tagsReply != nullptr);
    ignoreSsl(tagsReply);
    connect(tagsReply, &QNetworkReply::finished, this, [this, tagsReply]()
    {
        tagsReply->deleteLater();
        if (tagsReply->error() != QNetworkReply::NoError)
        {
            return;
        }
        m_tagsData = QJsonDocument::fromJson(tagsReply->readAll()).object()[QStringLiteral("results")].toArray();
        m_tagsList->clear();
        m_tagsList->setEnabled(true);
        for (const auto &t : m_tagsData)
        {
            auto *item = new QListWidgetItem(t.toObject()[QStringLiteral("name")].toString(), m_tagsList);
            Q_ASSERT(item != nullptr);
            item->setData(Qt::UserRole, t.toObject()[QStringLiteral("id")].toInt());
        }
    });

    // Korrespondenten
    auto *corrReply = m_nam->get(plRequest(QStringLiteral("/api/correspondents/?page_size=100")));
    Q_ASSERT(corrReply != nullptr);
    ignoreSsl(corrReply);
    connect(corrReply, &QNetworkReply::finished, this, [this, corrReply]()
    {
        corrReply->deleteLater();
        if (corrReply->error() != QNetworkReply::NoError)
        {
            return;
        }
        m_corrData = QJsonDocument::fromJson(corrReply->readAll()).object()[QStringLiteral("results")].toArray();
        m_corrCombo->clear();
        m_corrCombo->setEnabled(true);
        m_corrCombo->addItem(tr("— keiner —"), -1);
        for (const auto &c : m_corrData)
        {
            m_corrCombo->addItem(c.toObject()[QStringLiteral("name")].toString(), c.toObject()[QStringLiteral("id")].toInt());
        }
    });
}

QString PaperlessUploadDialog::title() const
{
    return m_titleEdit ? m_titleEdit->text().trimmed() : QString();
}

QList<int> PaperlessUploadDialog::selectedTags() const
{
    QList<int> ids;
    if (!m_tagsList)
    {
        return ids;
    }
    for (auto *item : m_tagsList->selectedItems())
    {
        ids << item->data(Qt::UserRole).toInt();
    }
    return ids;
}

int PaperlessUploadDialog::selectedCorrespondent() const
{
    return m_corrCombo ? m_corrCombo->currentData().toInt() : -1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statischer Upload (aus Rechtsklick)
// ═══════════════════════════════════════════════════════════════════════════════
void PaperlessManagerDialog::uploadFiles(const QStringList &files, QWidget *parent)
{
    if (Config::paperlessUrl().isEmpty() || Config::paperlessToken().isEmpty())
    {
        QMessageBox::warning(parent, tr("Paperless"), tr("Bitte zuerst URL und Token im Paperless Manager konfigurieren."));
        return;
    }

    PaperlessUploadDialog dlg(files, parent);
    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }

    auto *nam = new QNetworkAccessManager(parent);
    Q_ASSERT(nam != nullptr);
    const QList<int> tags = dlg.selectedTags();
    const int correspondent = dlg.selectedCorrespondent();
    const QString titleText = dlg.title();
    int *pending = new int(files.size());

    for (const QString &filePath : files)
    {
        QFile *file = new QFile(filePath);
        Q_ASSERT(file != nullptr);
        if (!file->open(QIODevice::ReadOnly))
        {
            delete file;
            --(*pending);
            continue;
        }

        auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        Q_ASSERT(multiPart != nullptr);

        // Datei-Part
        QHttpPart docPart;
        const QString mime = QMimeDatabase().mimeTypeForFile(filePath).name();
        docPart.setHeader(QNetworkRequest::ContentTypeHeader, mime);
        docPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"document\"; filename=\"%1\"").arg(QFileInfo(filePath).fileName()));
        docPart.setBodyDevice(file);
        file->setParent(multiPart);
        multiPart->append(docPart);

        // Titel
        const QString t = titleText.isEmpty() ? QFileInfo(filePath).completeBaseName() : titleText;
        QHttpPart titlePart;
        titlePart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"title\""));
        titlePart.setBody(t.toUtf8());
        multiPart->append(titlePart);

        // Tags
        for (int tagId : tags)
        {
            QHttpPart tagPart;
            tagPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"tags\""));
            tagPart.setBody(QByteArray::number(tagId));
            multiPart->append(tagPart);
        }

        // Korrespondent
        if (correspondent > 0)
        {
            QHttpPart corrPart;
            corrPart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"correspondent\""));
            corrPart.setBody(QByteArray::number(correspondent));
            multiPart->append(corrPart);
        }

        QNetworkRequest req(QUrl(Config::paperlessUrl() + QStringLiteral("/api/documents/post_document/")));
        req.setRawHeader("Authorization", QByteArray("Token ") + Config::paperlessToken().toUtf8());

        auto *reply = nam->post(req, multiPart);
        Q_ASSERT(reply != nullptr);
        multiPart->setParent(reply);
        ignoreSsl(reply);

        connect(reply, &QNetworkReply::finished, parent, [reply, pending, parent, filePath]()
        {
            reply->deleteLater();
            --(*pending);
            if (reply->error() != QNetworkReply::NoError)
            {
                QMessageBox::warning(parent, tr("Paperless Upload"), tr("Fehler beim Hochladen von %1:\n%2").arg(QFileInfo(filePath).fileName(), reply->errorString()));
            }
            if (*pending == 0)
            {
                delete pending;
                QMessageBox::information(parent, tr("Paperless"), tr("Upload abgeschlossen."));
            }
        });
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Manager-Dialog
// ═══════════════════════════════════════════════════════════════════════════════
PaperlessManagerDialog::PaperlessManagerDialog(const QString &currentPath, QWidget *parent)
    : QDialog(parent)
    , m_currentPath(currentPath)
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle(tr("Paperless Manager"));
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    setMinimumWidth(860);
    buildUI();
    adjustSize();
    loadDocuments();
}

void PaperlessManagerDialog::buildUI()
{
    const ThemeManager &tm = TM();

    const QString inputSS = QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:2px 8px; font-size:13px; min-height:22px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
    const QString labelSS = QString("color:%1; font-size:12px; font-weight:bold;")
        .arg(tm.colors().textPrimary);
    const QString primBtnSS = QString(
        "QPushButton { border-radius:3px; padding:3px 16px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:none; }"
        "QPushButton:hover { background:%3; }")
        .arg(tm.colors().accent, tm.colors().textLight, tm.colors().accentHover);
    const QString normBtnSS = QString(
        "QPushButton { border-radius:3px; padding:3px 12px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:1px solid %3; }"
        "QPushButton:hover { background:%4; }")
        .arg(tm.colors().bgPanel, tm.colors().textPrimary,
             tm.colors().borderAlt, tm.colors().bgHover);

    auto *root = new QVBoxLayout(this);
    Q_ASSERT(root != nullptr);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(6);

    setupConnectionSection(root, inputSS, normBtnSS, tm.colors().textAccent, tm.colors().borderAlt);
    setupSearchSection(root, inputSS, primBtnSS, normBtnSS);
    setupDocumentListSection(root, labelSS, tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
    setupProgressStatusSection(root, tm.colors().bgInput, tm.colors().borderAlt, tm.colors().accent, tm.colors().textMuted);
    setupFooterSection(root, normBtnSS);
}

void PaperlessManagerDialog::setupConnectionSection(QVBoxLayout *root, const QString &inputSS, const QString &normBtnSS, const QString &textAccent, const QString &borderAlt)
{
    Q_ASSERT(root != nullptr);
    auto *cfgBox = new QGroupBox(tr("Verbindung"), this);
    Q_ASSERT(cfgBox != nullptr);
    cfgBox->setStyleSheet(QString(
        "QGroupBox { color:%1; font-size:11px; border:1px solid %2; "
        "border-radius:4px; margin-top:6px; padding-top:4px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; }")
        .arg(textAccent, borderAlt));
    auto *cfgLay = new QFormLayout(cfgBox);
    Q_ASSERT(cfgLay != nullptr);
    cfgLay->setSpacing(6);
    cfgLay->setContentsMargins(8, 8, 8, 8);

    m_urlEdit = new QLineEdit(cfgBox);
    Q_ASSERT(m_urlEdit != nullptr);
    m_urlEdit->setStyleSheet(inputSS);
    m_urlEdit->setPlaceholderText(QStringLiteral("http://192.168.0.21:8000"));
    m_urlEdit->setText(Config::paperlessUrl());
    cfgLay->addRow(tr("URL:"), m_urlEdit);

    auto *tokenRow = new QHBoxLayout();
    Q_ASSERT(tokenRow != nullptr);
    m_tokenEdit = new QLineEdit(cfgBox);
    Q_ASSERT(m_tokenEdit != nullptr);
    m_tokenEdit->setStyleSheet(inputSS);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setText(Config::paperlessToken());
    m_tokenVisibleBtn = new QPushButton(cfgBox);
    Q_ASSERT(m_tokenVisibleBtn != nullptr);
    m_tokenVisibleBtn->setIcon(QIcon::fromTheme(QStringLiteral("password-show-on")));
    m_tokenVisibleBtn->setStyleSheet(normBtnSS);
    m_tokenVisibleBtn->setCheckable(true);
    m_tokenVisibleBtn->setMinimumHeight(28);
    m_tokenVisibleBtn->setToolTip(tr("Token anzeigen"));
    tokenRow->addWidget(m_tokenEdit, 1);
    tokenRow->addWidget(m_tokenVisibleBtn);
    cfgLay->addRow(tr("API-Token:"), tokenRow);

    m_sslIgnoreCheck = new QCheckBox(tr("SSL-Zertifikat nicht prüfen (für selbstsignierte Zertifikate)"), cfgBox);
    Q_ASSERT(m_sslIgnoreCheck != nullptr);
    m_sslIgnoreCheck->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(TM().colors().textPrimary));
    m_sslIgnoreCheck->setChecked(Config::paperlessSslIgnore());
    cfgLay->addRow(QString(), m_sslIgnoreCheck);

    auto *saveBtn = new QPushButton(tr("Speichern"), cfgBox);
    Q_ASSERT(saveBtn != nullptr);
    saveBtn->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    saveBtn->setStyleSheet(normBtnSS);
    cfgLay->addRow(QString(), saveBtn);

    root->addWidget(cfgBox);

    connect(m_tokenVisibleBtn, &QPushButton::toggled, this, [this](bool visible)
    {
        m_tokenEdit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
        m_tokenVisibleBtn->setIcon(QIcon::fromTheme(visible ? QStringLiteral("password-show-off") : QStringLiteral("password-show-on")));
    });
    connect(saveBtn, &QPushButton::clicked, this, &PaperlessManagerDialog::saveSettings);
}

void PaperlessManagerDialog::setupSearchSection(QVBoxLayout *root, const QString &inputSS, const QString &primBtnSS, const QString &normBtnSS)
{
    Q_ASSERT(root != nullptr);
    auto *searchRow = new QHBoxLayout();
    Q_ASSERT(searchRow != nullptr);
    m_searchEdit = new QLineEdit(this);
    Q_ASSERT(m_searchEdit != nullptr);
    m_searchEdit->setStyleSheet(inputSS);
    m_searchEdit->setPlaceholderText(tr("Dokumente suchen…"));
    m_searchEdit->setClearButtonEnabled(true);

    auto *searchBtn = new QPushButton(tr("Suchen"), this);
    Q_ASSERT(searchBtn != nullptr);
    searchBtn->setIcon(QIcon::fromTheme(QStringLiteral("search")));
    searchBtn->setStyleSheet(normBtnSS);

    auto *uploadBtn = new QPushButton(tr("Hochladen"), this);
    Q_ASSERT(uploadBtn != nullptr);
    uploadBtn->setIcon(QIcon::fromTheme(QStringLiteral("cloud-upload")));
    uploadBtn->setStyleSheet(primBtnSS);

    auto *refreshBtn = new QPushButton(this);
    Q_ASSERT(refreshBtn != nullptr);
    refreshBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    refreshBtn->setStyleSheet(normBtnSS);
    refreshBtn->setToolTip(tr("Aktualisieren"));

    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(searchBtn);
    searchRow->addWidget(uploadBtn);
    searchRow->addWidget(refreshBtn);
    root->addLayout(searchRow);

    connect(searchBtn, &QPushButton::clicked, this, [this]()
    {
        loadDocuments(m_searchEdit->text().trimmed());
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]()
    {
        loadDocuments(m_searchEdit->text().trimmed());
    });
    connect(refreshBtn, &QPushButton::clicked, this, [this]()
    {
        loadDocuments(m_searchEdit->text().trimmed());
    });
    connect(uploadBtn, &QPushButton::clicked, this, [this]()
    {
        QStringList files = QFileDialog::getOpenFileNames(this, tr("Dateien für Paperless auswählen"), m_currentPath);
        if (!files.isEmpty())
        {
            PaperlessManagerDialog::uploadFiles(files, this);
        }
    });
}

void PaperlessManagerDialog::setupDocumentListSection(QVBoxLayout *root, const QString &labelSS, const QString &bgInput, const QString &textPrimary, const QString &borderAlt)
{
    Q_ASSERT(root != nullptr);
    auto *listLbl = new QLabel(tr("Dokumente:"), this);
    Q_ASSERT(listLbl != nullptr);
    listLbl->setStyleSheet(labelSS);
    root->addWidget(listLbl);

    m_docList = new QListWidget(this);
    Q_ASSERT(m_docList != nullptr);
    m_docList->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "font-size:12px;")
        .arg(bgInput, textPrimary, borderAlt));
    m_docList->setAlternatingRowColors(true);
    root->addWidget(m_docList, 1);

    connect(m_docList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item)
    {
        const int id = item->data(Qt::UserRole).toInt();
        const QString fname = item->data(Qt::UserRole + 1).toString();
        downloadAndOpen(id, fname);
    });
}

void PaperlessManagerDialog::setupProgressStatusSection(QVBoxLayout *root, const QString &bgInput, const QString &borderAlt, const QString &accentColor, const QString &textMuted)
{
    Q_ASSERT(root != nullptr);
    m_progress = new QProgressBar(this);
    Q_ASSERT(m_progress != nullptr);
    m_progress->setVisible(false);
    m_progress->setMaximum(0); // Indeterminate
    m_progress->setStyleSheet(QString(
        "QProgressBar { border:1px solid %1; border-radius:3px; background:%2; "
        "max-height:6px; } "
        "QProgressBar::chunk { background:%3; border-radius:3px; }")
        .arg(borderAlt, bgInput, accentColor));
    root->addWidget(m_progress);

    m_statusLabel = new QLabel(this);
    Q_ASSERT(m_statusLabel != nullptr);
    m_statusLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(textMuted));
    root->addWidget(m_statusLabel);
}

void PaperlessManagerDialog::setupFooterSection(QVBoxLayout *root, const QString &normBtnSS)
{
    Q_ASSERT(root != nullptr);
    const QString sepSS = QStringLiteral("background:%1; max-height:1px; margin:6px 0;").arg(TM().colors().borderAlt);
    auto *sep = new QFrame(this);
    Q_ASSERT(sep != nullptr);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(sepSS);
    root->addWidget(sep);

    auto *footer = new QHBoxLayout();
    Q_ASSERT(footer != nullptr);
    auto *openWebBtn = new QPushButton(tr("Im Browser öffnen"), this);
    Q_ASSERT(openWebBtn != nullptr);
    openWebBtn->setIcon(QIcon::fromTheme(QStringLiteral("internet-web-browser")));
    openWebBtn->setStyleSheet(normBtnSS);
    auto *closeBtn = new QPushButton(tr("Schließen"), this);
    Q_ASSERT(closeBtn != nullptr);
    closeBtn->setStyleSheet(normBtnSS);
    footer->addWidget(openWebBtn);
    footer->addStretch();
    footer->addWidget(closeBtn);
    root->addLayout(footer);

    connect(openWebBtn, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(Config::paperlessUrl() + QStringLiteral("/dashboard")));
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void PaperlessManagerDialog::loadDocuments(const QString &query)
{
    if (Config::paperlessUrl().isEmpty() || Config::paperlessToken().isEmpty())
    {
        m_statusLabel->setText(tr("Bitte URL und Token konfigurieren."));
        return;
    }

    m_progress->setVisible(true);
    m_statusLabel->setText(tr("Lädt…"));
    m_docList->clear();

    QString endpoint = QStringLiteral("/api/documents/?page_size=50&ordering=-created");
    if (!query.isEmpty())
    {
        endpoint += QStringLiteral("&search=") + QUrl::toPercentEncoding(query);
    }

    auto *reply = m_nam->get(plRequest(endpoint));
    Q_ASSERT(reply != nullptr);
    ignoreSsl(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        reply->deleteLater();
        m_progress->setVisible(false);

        if (reply->error() != QNetworkReply::NoError)
        {
            m_statusLabel->setText(tr("Fehler: %1").arg(reply->errorString()));
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        m_documents = doc.object()[QStringLiteral("results")].toArray();
        const int total = doc.object()[QStringLiteral("count")].toInt();

        m_docList->clear();
        for (const auto &d : m_documents)
        {
            const auto obj = d.toObject();
            const int id = obj[QStringLiteral("id")].toInt();
            const QString title = obj[QStringLiteral("title")].toString();
            const QString created = obj[QStringLiteral("created")].toString().left(10);
            const QString origName = obj[QStringLiteral("original_file_name")].toString();

            auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("application-pdf")), QStringLiteral("%1  —  %2").arg(created, title), m_docList);
            Q_ASSERT(item != nullptr);
            item->setData(Qt::UserRole, id);
            item->setData(Qt::UserRole + 1, origName.isEmpty() ? title + QStringLiteral(".pdf") : origName);
            item->setToolTip(origName);
        }

        m_statusLabel->setText(tr("%1 Dokument(e) — %2 angezeigt").arg(total).arg(m_documents.size()));
    });
}

void PaperlessManagerDialog::downloadAndOpen(int docId, const QString &filename)
{
    m_progress->setVisible(true);
    m_statusLabel->setText(tr("Lade %1 herunter…").arg(filename));

    const QString endpoint = QStringLiteral("/api/documents/%1/download/").arg(docId);
    auto *reply = m_nam->get(plRequest(endpoint));
    Q_ASSERT(reply != nullptr);

    connect(reply, &QNetworkReply::finished, this, [this, reply, filename]()
    {
        reply->deleteLater();
        m_progress->setVisible(false);

        if (reply->error() != QNetworkReply::NoError)
        {
            m_statusLabel->setText(tr("Fehler: %1").arg(reply->errorString()));
            return;
        }

        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString tmpPath = tmpDir + QStringLiteral("/") + filename;

        QFile f(tmpPath);
        if (f.open(QIODevice::WriteOnly))
        {
            f.write(reply->readAll());
            f.close();
            QDesktopServices::openUrl(QUrl::fromLocalFile(tmpPath));
            m_statusLabel->setText(tr("%1 geöffnet.").arg(filename));
        }
        else
        {
            m_statusLabel->setText(tr("Konnte Datei nicht schreiben."));
        }
    });
}

void PaperlessManagerDialog::saveSettings()
{
    Config::setPaperlessUrl(m_urlEdit->text().trimmed().remove('/').isEmpty() ? QString() : m_urlEdit->text().trimmed());

    // Trailing slash entfernen
    QString url = m_urlEdit->text().trimmed();
    if (url.endsWith('/'))
    {
        url.chop(1);
    }
    Config::setPaperlessUrl(url);
    Config::setPaperlessToken(m_tokenEdit->text().trimmed());
    if (m_sslIgnoreCheck != nullptr)
    {
        Config::setPaperlessSslIgnore(m_sslIgnoreCheck->isChecked());
    }
    m_statusLabel->setText(tr("Einstellungen gespeichert."));
    loadDocuments(m_searchEdit->text().trimmed());
}
