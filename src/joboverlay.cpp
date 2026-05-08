#include "joboverlay.h"
#include "thememanager.h"
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QApplication>
#include <QTimer>

JobOverlay::JobOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    hide();

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(8);

    auto *topLay = new QHBoxLayout();
    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 11px;");
    
    m_cancelBtn = new QPushButton(QIcon::fromTheme("dialog-cancel"), QString(), this);
    m_cancelBtn->setFixedSize(20, 20);
    m_cancelBtn->setFlat(true);
    connect(m_cancelBtn, &QPushButton::clicked, this, &JobOverlay::cancelJob);

    topLay->addWidget(m_titleLabel);
    topLay->addStretch();
    topLay->addWidget(m_cancelBtn);
    mainLay->addLayout(topLay);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("font-size: 10px;");
    mainLay->addWidget(m_infoLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    mainLay->addWidget(m_progressBar);

    setFixedWidth(280);
    setFixedHeight(85);

    connect(&TM(), &ThemeManager::themeChanged, this, &JobOverlay::updateStyling);
    updateStyling();
}

void JobOverlay::addJob(KJob *job, const QString &title)
{
    m_currentJob = job;
    m_titleLabel->setText(title);
    m_infoLabel->setText(tr("Initialisierung..."));
    m_progressBar->setValue(0);
    
    connect(job, &KJob::percentChanged, this, &JobOverlay::updateProgress);
    connect(job, &KJob::result, this, &JobOverlay::jobFinished);
    
    show();
    updatePosition();
    
    // Kleine Einblend-Animation
    auto *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(300);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void JobOverlay::updateProgress(KJob *job, unsigned long percent)
{
    Q_UNUSED(job)
    m_progressBar->setValue(static_cast<int>(percent));
    m_infoLabel->setText(tr("Fortschritt: %1%").arg(percent));
}

void JobOverlay::jobFinished(KJob *job)
{
    if (job->error()) {
        m_infoLabel->setText(tr("Fehler: %1").arg(job->errorString()));
        m_progressBar->setStyleSheet("QProgressBar::chunk { background: #ff5555; }");
    } else {
        m_infoLabel->setText(tr("Fertig!"));
        m_progressBar->setValue(100);
    }

    // Nach 2 Sekunden ausblenden
    QTimer::singleShot(2000, this, [this]() {
        auto *anim = new QPropertyAnimation(this, "windowOpacity");
        anim->setDuration(500);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, this, &JobOverlay::hide);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void JobOverlay::cancelJob()
{
    if (m_currentJob) {
        m_currentJob->kill(KJob::EmitResult);
    }
    hide();
}

void JobOverlay::updateStyling()
{
    const auto &c = TM().colors();
    QString style = QString(
        "JobOverlay { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
        "QLabel { color: %3; }"
        "QProgressBar { background: %4; border: none; border-radius: 3px; }"
        "QProgressBar::chunk { background: %5; border-radius: 3px; }"
    ).arg(c.bgBox, c.border, c.textPrimary, c.bgDeep, c.accent);
    
    setStyleSheet(style);
}

void JobOverlay::updatePosition()
{
    if (!parentWidget()) return;
    int margin = 20;
    move(parentWidget()->width() - width() - margin,
         parentWidget()->height() - height() - margin);
}

void JobOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    // Schatten-Effekt (vereinfacht)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawRoundedRect(rect().adjusted(2, 2, 0, 0), 8, 8);
}

void JobOverlay::resizeEvent(QResizeEvent *)
{
    updatePosition();
}
