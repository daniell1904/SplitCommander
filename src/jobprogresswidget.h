#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QTimer>
#include <QFrame>

class JobProgressWidget : public QFrame {
    Q_OBJECT
public:
    explicit JobProgressWidget(QWidget *parent = nullptr) : QFrame(parent) {
        setFixedWidth(250);
        setFixedHeight(60);
        setStyleSheet("JobProgressWidget { background: #2e3440; border: 1px solid #4c566a; border-radius: 8px; } "
                      "QLabel { color: #d8dee9; font-size: 11px; } "
                      "QProgressBar { background: #3b4252; border: none; height: 4px; text-align: transparent; } "
                      "QProgressBar::chunk { background: #88c0d0; }");
        
        auto *vl = new QVBoxLayout(this);
        m_label = new QLabel("Bereite vor...", this);
        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 100);
        
        vl->addWidget(m_label);
        vl->addWidget(m_progress);
        hide();
    }

public slots:
    void updateStatus(const QString &file, int value) {
        show();
        m_label->setText(tr("Kopiere: %1").arg(file));
        m_progress->setValue(value);
    }

    void finish() {
        m_label->setText("Fertig!");
        m_progress->setValue(100);
        QTimer::singleShot(2000, this, &QWidget::hide);
    }

private:
    QLabel *m_label;
    QProgressBar *m_progress;
};