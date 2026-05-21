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

// ─── Upload-Dialog ────────────────────────────────────────────────────────────
class PaperlessUploadDialog : public QDialog {
    Q_OBJECT
public:
    explicit PaperlessUploadDialog(const QStringList &files, QWidget *parent = nullptr);
    QString title() const;
    QList<int> selectedTags() const;
    int selectedCorrespondent() const;

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
class PaperlessManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PaperlessManagerDialog(const QString &currentPath,
                                    QWidget *parent = nullptr);

    // Statische Hilfsfunktion für Upload aus Rechtsklick
    static void uploadFiles(const QStringList &files, QWidget *parent);

private:
    void buildUI();
    void loadDocuments(const QString &query = QString());
    void downloadAndOpen(int docId, const QString &filename);
    void saveSettings();

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
