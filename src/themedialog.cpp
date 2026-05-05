#include "themedialog.h"
#include "settingsdialog.h"
#include "thememanager.h"

#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QFrame>

ThemeDialog::ThemeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Theme auswählen"));
    setMinimumWidth(420);
    setStyleSheet(TM().ssDialog());

    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    // ── Inhalt ────────────────────────────────────────────────────────────
    auto *content = new QWidget(this);
    auto *lay     = new QVBoxLayout(content);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    // System-Theme
    auto *grpSys = new QGroupBox(tr("System-Theme"), content);
    auto *sysLay = new QVBoxLayout(grpSys);
    m_sysCheck   = new QCheckBox(tr("KDE Global Theme verwenden"), grpSys);
    m_sysCheck->setChecked(SettingsDialog::useSystemTheme());
    auto *sysHint = new QLabel(tr("Übernimmt Farben und Stil des aktiven KDE Global Themes."), grpSys);
    sysHint->setObjectName("hint");
    sysHint->setWordWrap(true);
    sysLay->addWidget(m_sysCheck);
    sysLay->addWidget(sysHint);
    lay->addWidget(grpSys);

    // Eigene Themes
    m_themeBox   = new QGroupBox(tr("Eigenes Theme"), content);
    auto *themeLay = new QVBoxLayout(m_themeBox);
    themeLay->setSpacing(8);
    m_themeGroup = new QButtonGroup(m_themeBox);

    const QString curTheme = SettingsDialog::selectedTheme();

    for (int i = 0; i < SD_Styles::THEMES.size(); ++i) {
        const auto &t = SD_Styles::THEMES.at(i);

        auto *card = new QWidget(m_themeBox);
        card->setFixedHeight(56);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(QString(
            "QWidget { background:%1; border-radius:6px; border:1px solid rgba(255,255,255,12); }")
            .arg(t.bg));

        auto *cardLay = new QHBoxLayout(card);
        cardLay->setContentsMargins(12, 0, 12, 0);
        cardLay->setSpacing(10);

        auto *rb = new QRadioButton(card);
        rb->setStyleSheet("QRadioButton { background:transparent; border:none; }"
                          "QRadioButton::indicator { width:14px; height:14px; }");
        m_themeGroup->addButton(rb, i);
        cardLay->addWidget(rb);

        auto *nameLabel = new QLabel(t.name, card);
        nameLabel->setStyleSheet(QString(
            "color:%1; font-size:12px; font-weight:600; background:transparent; border:none;")
            .arg(t.text));
        cardLay->addWidget(nameLabel, 1);

        // Farbchips
        for (const QString &col : { t.bg, t.box, t.accent, t.text }) {
            auto *chip = new QLabel(card);
            chip->setFixedSize(28, 28);
            QPixmap px(28, 28);
            px.fill(Qt::transparent);
            QPainter pp(&px);
            pp.setRenderHint(QPainter::Antialiasing);
            pp.setBrush(QColor(col));
            pp.setPen(QPen(QColor(255,255,255,20), 1));
            pp.drawRoundedRect(1, 1, 26, 26, 5, 5);
            chip->setPixmap(px);
            cardLay->addWidget(chip);
        }

        // Karte klickbar machen
        struct CardFilter : public QObject {
            QRadioButton *btn;
            CardFilter(QObject *p, QRadioButton *b) : QObject(p), btn(b) {}
            bool eventFilter(QObject *, QEvent *e) override {
                if (e->type() == QEvent::MouseButtonPress) { btn->setChecked(true); return true; }
                return false;
            }
        };
        auto *cf = new CardFilter(card, rb);
        card->installEventFilter(cf);
        for (auto *child : card->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (child != rb) child->installEventFilter(cf);

        // Rahmen bei Auswahl
        connect(rb, &QRadioButton::toggled, card, [card, t](bool checked) {
            if (checked)
                card->setStyleSheet(QString(
                    "QWidget { background:%1; border-radius:6px; border:2px solid %2; }").arg(t.bg, t.accent));
            else
                card->setStyleSheet(QString(
                    "QWidget { background:%1; border-radius:6px; border:1px solid rgba(255,255,255,12); }").arg(t.bg));
        });

        if (t.name == curTheme)
            rb->setChecked(true);

        themeLay->addWidget(card);
    }

    connect(m_sysCheck, &QCheckBox::toggled, m_themeBox, &QWidget::setDisabled);
    m_themeBox->setDisabled(SettingsDialog::useSystemTheme());
    lay->addWidget(m_themeBox);
    lay->addStretch();
    outerLay->addWidget(content, 1);

    // ── Bottom-Bar ────────────────────────────────────────────────────────
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
    connect(m_applyBtn, &QPushButton::clicked, this, &ThemeDialog::applyAndSave);
    bottomLay->addWidget(m_applyBtn);

    outerLay->addWidget(bottomBar);
}

void ThemeDialog::applyAndSave()
{
    QSettings s("SplitCommander", "Appearance");
    const bool useSys = m_sysCheck->isChecked();
    s.setValue("useSystemTheme", useSys);

    if (!useSys) {
        int idx = m_themeGroup->checkedId();
        if (idx >= 0 && idx < SD_Styles::THEMES.size())
            s.setValue("theme", SD_Styles::THEMES.at(idx).name);
    }
    s.sync();

    accept();

    QMessageBox::information(nullptr, tr("Neustart erforderlich"),
        tr("SplitCommander wird jetzt neu gestartet."));
    QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments());
    QApplication::quit();
}
