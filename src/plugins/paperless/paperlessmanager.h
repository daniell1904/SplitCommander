#pragma once

#include <QDialog>
#include <QWidget>
#include <QJsonArray>

class QLineEdit;
class QTextEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QComboBox;
class QLabel;
class QCheckBox;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QVBoxLayout;

// ─── Upload-Dialog ────────────────────────────────────────────────────────────
class PaperlessUploadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaperlessUploadDialog(const QStringList &files, QWidget *parent = nullptr);
    ~PaperlessUploadDialog() override = default;

    [[nodiscard]] QString title() const;
    [[nodiscard]] QList<int> selectedTags() const;
    [[nodiscard]] int selectedCorrespondent() const;

private:
    void loadMeta();
    QLineEdit       *m_titleEdit    = nullptr;
    QListWidget     *m_tagsList     = nullptr;
    QComboBox       *m_corrCombo    = nullptr;
    QNetworkAccessManager *m_nam    = nullptr;
    QJsonArray       m_tagsData;
    QJsonArray       m_corrData;
};

// ─── Manager-Dialog ───────────────────────────────────────────────────────────
class PaperlessManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaperlessManagerDialog(const QString &currentPath, QWidget *parent = nullptr);
    ~PaperlessManagerDialog() override = default;

    // Statische Hilfsfunktion für Upload aus Rechtsklick
    static void uploadFiles(const QStringList &files, QWidget *parent);

private:
    void buildUI();
    void loadDocuments(const QString &query = QString());
    void downloadAndOpen(int docId, const QString &filename);
    void saveSettings();

    // UI builders (NASA Rule 4 Compliance)
    void setupConnectionSection(QVBoxLayout *root, const QString &inputSS, const QString &normBtnSS, const QString &textAccent, const QString &borderAlt);
    void setupSearchSection(QVBoxLayout *root, const QString &inputSS, const QString &primBtnSS, const QString &normBtnSS);
    void setupDocumentListSection(QVBoxLayout *root, const QString &labelSS, const QString &bgInput, const QString &textPrimary, const QString &borderAlt);
    void setupProgressStatusSection(QVBoxLayout *root, const QString &bgInput, const QString &borderAlt, const QString &accentColor, const QString &textMuted);
    void setupFooterSection(QVBoxLayout *root, const QString &normBtnSS);

    QString m_currentPath;
    QNetworkAccessManager *m_nam    = nullptr;

    QLineEdit   *m_urlEdit          = nullptr;
    QLineEdit   *m_tokenEdit        = nullptr;
    QPushButton *m_tokenVisibleBtn  = nullptr;
    QLineEdit   *m_searchEdit       = nullptr;
    QListWidget *m_docList          = nullptr;
    QLabel      *m_statusLabel      = nullptr;
    QProgressBar *m_progress        = nullptr;
    QCheckBox    *m_sslIgnoreCheck  = nullptr;

    QJsonArray   m_documents;
};
