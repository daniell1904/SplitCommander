#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <QFutureWatcher>
#include <QImage>

// --- PreviewResult --- (Datenstruktur für asynchrone Vorschaudaten)
struct PreviewResult {
    QString path;
    QImage  image;
    QString text;
    QString meta;
    bool    isText = false;
};

// --- PreviewPanel --- (Vorschaubereich für Dateidetails (Bild, Text, etc.))
class PreviewPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget *parent = nullptr);
    void showFile(const QString &path);
    void clear();

private slots:
    void onPreviewReady();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void scalePixmap();

    QLabel    *m_imgLabel   = nullptr;
    QLabel    *m_metaLabel  = nullptr;
    QTextEdit *m_textEdit   = nullptr;
    QPixmap    m_storedPixmap;
    QFutureWatcher<PreviewResult> m_watcher;
};
