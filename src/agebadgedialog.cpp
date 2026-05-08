#include "agebadgedialog.h"
#include "settingsdialog.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QToolButton>
#include <QFrame>
#include <QPainter>
#include <QSettings>
#include <QIcon>

// --- Lokaler Gradient-Balken ---
class AgeBadgeGradBar : public QWidget {
public:
    QList<QColor> *cols;
    AgeBadgeGradBar(QWidget *p, QList<QColor> *c) : QWidget(p), cols(c) { setFixedHeight(52); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int w = width() - 4;
        QLinearGradient grad(2, 8, w+2, 8);
        for (int i = 0; i < 6; ++i)
            grad.setColorAt(i / 5.0, (*cols)[i]);
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRoundedRect(2, 8, w, 22, 4, 4);
        const QStringList lbl = {"▪1 Stunde","▪1 Tag","▪7 Tage","▪1 Monat","▪1 Jahr","▪>1 Jahr"};
        QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
        for (int i = 0; i < 6; ++i) {
            double pos = i / 5.0;
            int x = 2 + (int)(pos * w);
            QColor bg = (*cols)[i];
            p.setPen(bg.lightnessF() > 0.45 ? Qt::black : Qt::white);
            p.drawText(x+2, 24, lbl[i]);
        }
    }
};

// ---  ---
AgeBadgeDialog::AgeBadgeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Altersbadges"));
    setMinimumWidth(400);
    setStyleSheet(TM().ssDialog());

    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    // --- Inhalt ---
    auto *content = new QWidget(this);
    auto *lay     = new QVBoxLayout(content);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    // Farben laden
    for (int i = 0; i < 6; ++i)
        m_ageColors.append(SettingsDialog::ageBadgeColor(i));

    // GroupBox
    auto *grpAge = new QGroupBox(tr("Dateialter / relatives Datum"), content);
    auto *ageLay = new QVBoxLayout(grpAge);
    ageLay->setSpacing(6);

    // Gradient-Balken
    auto *gradBar = new AgeBadgeGradBar(grpAge, &m_ageColors);
    m_gradBar = gradBar;
    ageLay->addWidget(gradBar);

    // Slider
    QSettings ageS("SplitCommander", "AgeBadge");
    auto *sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(8);

    auto mkLabel = [&](const QString &t) {
        auto *l = new QLabel(t, grpAge);
        l->setFixedWidth(10);
        return l;
    };

    m_sSlider = new QSlider(Qt::Horizontal, grpAge);
    m_sSlider->setRange(0, 255);
    m_sSlider->setValue(ageS.value("saturation", 220).toInt());
    m_sSlider->setFixedHeight(18);

    m_lSlider = new QSlider(Qt::Horizontal, grpAge);
    m_lSlider->setRange(0, 255);
    m_lSlider->setValue(ageS.value("lightness", 140).toInt());
    m_lSlider->setFixedHeight(18);

    auto *resetBtn = new QToolButton(grpAge);
    resetBtn->setIcon(QIcon::fromTheme("edit-undo"));
    resetBtn->setToolTip(tr("Zurücksetzen"));
    resetBtn->setFixedSize(22, 22);

    sliderRow->addWidget(mkLabel("S"));
    sliderRow->addWidget(m_sSlider, 1);
    sliderRow->addWidget(mkLabel("L"));
    sliderRow->addWidget(m_lSlider, 1);
    sliderRow->addWidget(resetBtn);
    ageLay->addLayout(sliderRow);

    connect(m_sSlider, &QSlider::valueChanged, this, &AgeBadgeDialog::updateDynamicColors);
    connect(m_lSlider, &QSlider::valueChanged, this, &AgeBadgeDialog::updateDynamicColors);
    connect(resetBtn, &QToolButton::clicked, this, [this]() {
        m_sSlider->setValue(220);
        m_lSlider->setValue(140);
        updateDynamicColors();
    });

    // Checkbox: Neue Dateien hervorheben
    m_indicatorCheck = new QCheckBox(tr("Neue Dateien hervorheben (< 2 Tage)"), grpAge);
    m_indicatorCheck->setChecked(ageS.value("showNewIndicator", false).toBool());
    ageLay->addWidget(m_indicatorCheck);

    lay->addWidget(grpAge);
    lay->addStretch();
    outerLay->addWidget(content, 1);

    // --- Bottom-Bar ---
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("background:%1;").arg(TM().colors().separator));
    sep->setFixedHeight(1);
    outerLay->addWidget(sep);

    auto *bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(52);
    bottomBar->setStyleSheet(QString("background:%1;").arg(TM().colors().bgPanel));
    auto *bottomLay = new QHBoxLayout(bottomBar);
    bottomLay->setContentsMargins(16, 0, 16, 0);
    bottomLay->setSpacing(8);
    bottomLay->addStretch();

    auto *cancelBtn = new QPushButton(tr("Abbrechen"), bottomBar);
    cancelBtn->setFixedWidth(100);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomLay->addWidget(cancelBtn);

    m_applyBtn = new QPushButton(tr("Übernehmen"), bottomBar);
    m_applyBtn->setObjectName("applyBtn");
    m_applyBtn->setFixedWidth(120);
    m_applyBtn->setDefault(true);
    connect(m_applyBtn, &QPushButton::clicked, this, &AgeBadgeDialog::applyAndSave);
    bottomLay->addWidget(m_applyBtn);

    outerLay->addWidget(bottomBar);
}

void AgeBadgeDialog::updateDynamicColors()
{
    if (!m_sSlider || !m_lSlider) return;

    int sMapped = 40 + (m_sSlider->value() * (255 - 40) / 255);
    int lMapped = 60 + (m_lSlider->value() * (220 - 60) / 255);

    const int hues[6] = {0, 30, 80, 160, 220, 270};
    for (int i = 0; i < 6; ++i) {
        int s_final = (i == 5) ? sMapped / 2 : sMapped;
        if (i < m_ageColors.size())
            m_ageColors[i] = QColor::fromHsl(hues[i], s_final, lMapped);
    }

    if (m_gradBar) m_gradBar->update();
}

void AgeBadgeDialog::applyAndSave()
{
    QSettings s("SplitCommander", "AgeBadge");
    s.setValue("saturation", m_sSlider->value());
    s.setValue("lightness",  m_lSlider->value());
    s.setValue("showNewIndicator", m_indicatorCheck->isChecked());
    s.sync();
    accept();
}

bool AgeBadgeDialog::showNewIndicator()
{
    QSettings s("SplitCommander", "AgeBadge");
    return s.value("showNewIndicator", false).toBool();
}
