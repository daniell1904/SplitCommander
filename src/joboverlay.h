#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <KIO/Job>

class JobOverlay : public QWidget {
    Q_OBJECT

public:
    explicit JobOverlay(QWidget *parent = nullptr);
    void addJob(KJob *job, const QString &title);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateProgress(KJob *job, unsigned long percent);
    void jobFinished(KJob *job);
    void cancelJob();

private:
    void updateStyling();
    void updatePosition();

    QLabel *m_titleLabel;
    QLabel *m_infoLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelBtn;
    KJob *m_currentJob = nullptr;
};
