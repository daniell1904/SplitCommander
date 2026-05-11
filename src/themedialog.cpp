#include "themedialog.h"
#include "config.h"
#include "thememanager.h"

#include <QApplication>
#include "dialogutils.h"
#include <QProcess>
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

    // --- Inhalt ---
    auto *content = new QWidget(this);
    auto *lay     = new QVBoxLayout(content);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    // System-Theme
    auto *grpSys = new QGroupBox(tr("System-Theme"), content);
    auto *sysLay = new QVBoxLayout(grpSys);
    m_sysCheck   = new QCheckBox(tr("KDE Global Theme verwenden"), grpSys);
    m_sysCheck->setChecked(Config::useSystemTheme());
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

    const QString curTheme = Config::selectedTheme();

    const auto allThemes = TM().allThemes();
    for (int i = 0; i < allThemes.size(); ++i) {
        const auto &t = allThemes.at(i);

        auto *card = new QWidget(m_themeBox);
        card->setFixedHeight(56);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(QString(
            "QWidget { background:%1; border-radius:6px; border:1px solid rgba(255,255,255,12); }")
            .arg(t.bgMain));

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
            .arg(t.textPrimary));
        cardLay->addWidget(nameLabel, 1);

        // Farbchips
        const QStringList cols = { t.bgMain, t.bgBox, t.accent, t.textPrimary };
        for (const QString &col : cols) {
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
                    "QWidget { background:%1; border-radius:6px; border:2px solid %2; }").arg(t.bgMain, t.accent));
            else
                card->setStyleSheet(QString(
                    "QWidget { background:%1; border-radius:6px; border:1px solid rgba(255,255,255,12); }").arg(t.bgMain));
        });

        if (t.name == curTheme)
            rb->setChecked(true);

        themeLay->addWidget(card);
    }

    connect(m_sysCheck, &QCheckBox::toggled, m_themeBox, &QWidget::setDisabled);
    m_themeBox->setDisabled(Config::useSystemTheme());
    lay->addWidget(m_themeBox);
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
    connect(m_applyBtn, &QPushButton::clicked, this, &ThemeDialog::applyAndSave);
    bottomLay->addWidget(m_applyBtn);

    outerLay->addWidget(bottomBar);
}

void ThemeDialog::applyAndSave()
{
    const bool useSys = m_sysCheck->isChecked();
    Config::setUseSystemTheme(useSys);

    if (!useSys) {
        int idx = m_themeGroup->checkedId();
        const auto allThemes = TM().allThemes();
        if (idx >= 0 && idx < allThemes.size())
            Config::setSelectedTheme(allThemes.at(idx).name);
    }

    accept();


    DialogUtils::message(nullptr, tr("Neustart erforderlich"),
        tr("Das Theme wurde erfolgreich importiert und wird nach einem Neustart von SplitCommander vollständig angewendet."));
    QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments());
    QApplication::quit();
}
