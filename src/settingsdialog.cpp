#include "settingsdialog.h"
#include "config.h"
#include "thememanager.h"
#include "mainwindow.h"
#include "themecreatordialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QInputDialog>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QScrollArea>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QPainter>
#include <QSlider>
#include <QApplication>
#include <QProcess>
#include <QMessageBox>
#include <QRegularExpression>
#include <QColorDialog>
#include <QFileDialog>

#include <KShortcutsEditor>
#include <KActionCollection>

// Local helper for gradient bar
class AgeBadgeGradBar : public QWidget
{
public:
    QList<QColor> *cols;
    
    AgeBadgeGradBar(QWidget *p, QList<QColor> *c) : QWidget(p), cols(c)
    {
        setFixedHeight(52);
    }
    
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int w = width() - 4;
        QLinearGradient grad(2, 8, w + 2, 8);
        for (int i = 0; i < 6; ++i)
        {
            grad.setColorAt(i / 5.0, (*cols)[i]);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRoundedRect(2, 8, w, 22, 4, 4);
        const QStringList lbl = {tr("1 Std"), tr("1 Tag"), tr("7 Tage"), tr("1 Monat"), tr("1 Jahr"), tr(">1 Jahr")};
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        for (int i = 0; i < 6; ++i)
        {
            double pos = i / 5.0;
            int x = 2 + static_cast<int>(pos * w);
            QColor bg = (*cols)[i];
            p.setPen(bg.lightnessF() > 0.45 ? Qt::black : Qt::white);
            p.drawText(x + 2, 24, lbl[i]);
        }
    }
};

// Local helper for warning-free colored cards
class ColorCard : public QWidget
{
public:
    QColor bg;
    QColor border;
    int radius;

    ColorCard(QWidget *p, QColor b, QColor br, int r)
        : QWidget(p), bg(b), border(br), radius(r)
    {
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(border, 1));
        p.setBrush(bg);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius, radius);
    }
};

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    setMinimumSize(850, 750);
    resize(900, 850);
    
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    
    buildUI();
    load();
}

void SettingsDialog::showPage(Page page)
{
    int idx = static_cast<int>(page);
    m_sidebar->setCurrentRow(idx);
    m_stack->setCurrentIndex(idx);
    show();
    raise();
    activateWindow();
}

void SettingsDialog::buildUI()
{
    auto *root = new QHBoxLayout(this);
    Q_ASSERT(root != nullptr);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    const auto &c = TM().colors();

    // SIDEBAR
    m_sidebar = new QListWidget();
    Q_ASSERT(m_sidebar != nullptr);
    m_sidebar->setFixedWidth(220);
    m_sidebar->setSpacing(6);
    m_sidebar->setStyleSheet(QString(
        "QListWidget { background: %1; border: none; border-right: 1px solid %2; padding: 15px; outline: none; }"
        "QListWidget::item {"
        "  background: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 10px 12px;"
        "  color: %4;"
        "  font-weight: 500;"
        "}"
        "QListWidget::item:hover { background: %5; }"
        "QListWidget::item:selected {"
        "  background: %6;"
        "  color: %7;"
        "  border: 1px solid %6;"
        "}"
    ).arg(c.bgPanel, c.borderAlt, c.bgBox, c.textPrimary, c.bgHover, c.accent, c.textLight));

    m_sidebar->addItem(tr("Allgemein"));
    m_sidebar->addItem(tr("Erscheinungsbild"));
    m_sidebar->addItem(tr("Kurzbefehle"));

    root->addWidget(m_sidebar);

    // STACK
    m_stack = new QStackedWidget();
    Q_ASSERT(m_stack != nullptr);
    m_stack->addWidget(createGeneralPage());
    m_stack->addWidget(createAppearancePage()); 
    m_stack->addWidget(createShortcutsPage());

    root->addWidget(m_stack, 1);

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

    auto *rightSide = new QVBoxLayout();
    Q_ASSERT(rightSide != nullptr);
    rightSide->addWidget(m_stack, 1);
    
    auto *footer = new QHBoxLayout();
    Q_ASSERT(footer != nullptr);
    footer->setContentsMargins(20, 10, 20, 20);
    auto *btnApply = new QPushButton(tr("Übernehmen & Neustarten"));
    Q_ASSERT(btnApply != nullptr);
    auto *btnClose = new QPushButton(tr("Schließen"));
    Q_ASSERT(btnClose != nullptr);
    
    const QString footerBtnBase = QStringLiteral("border-radius: 4px; padding: 4px 16px; font-size: 13px; font-weight: bold; min-height: 28px;");
    btnApply->setStyleSheet(QString("background:%1; color:%2; border:none; %3")
                        .arg(c.accent, c.textLight, footerBtnBase));
    btnClose->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; %4")
                        .arg(c.bgPanel, c.textPrimary, c.borderAlt, footerBtnBase));

    footer->addStretch();
    footer->addWidget(btnClose);
    footer->addWidget(btnApply);
    rightSide->addLayout(footer);
    
    root->addLayout(rightSide, 1);

    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
    connect(btnApply, &QPushButton::clicked, this, &SettingsDialog::save);
}

QWidget* SettingsDialog::createGeneralPage()
{
    auto *mainWidget = new QWidget();
    auto *mainLay = new QVBoxLayout(mainWidget);
    Q_ASSERT(mainLay != nullptr);
    mainLay->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea();
    Q_ASSERT(scroll != nullptr);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget();
    auto *lay = new QVBoxLayout(content);
    Q_ASSERT(lay != nullptr);
    lay->setContentsMargins(30, 30, 30, 30);
    lay->setSpacing(20);

    const auto &c = TM().colors();

    auto *lbl = new QLabel(tr("Allgemeine Einstellungen"));
    Q_ASSERT(lbl != nullptr);
    lbl->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(c.accent));
    lay->addWidget(lbl);

    setupLanguageSection(lay);
    setupStartupSection(lay);
    setupBehaviorSection(lay);
    setupDrivesSection(lay);
    setupDisplaySection(lay);
    setupBlacklistSection(lay);

    lay->addStretch();
    scroll->setWidget(content);
    mainLay->addWidget(scroll);
    return mainWidget;
}

void SettingsDialog::setupLanguageSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    const auto &c = TM().colors();
    auto *grpLang = new QGroupBox(tr("Sprache"));
    Q_ASSERT(grpLang != nullptr);
    auto *langForm = new QFormLayout(grpLang);
    Q_ASSERT(langForm != nullptr);
    m_languageCombo = new QComboBox();
    Q_ASSERT(m_languageCombo != nullptr);
    m_languageCombo->setMaxVisibleItems(25);
    
    struct LangEntry
    {
        QString code;
        QString display;
    };
    const QList<LangEntry> langs = {
        {QStringLiteral(""),      tr("Systemsprache")},
        {QStringLiteral("cs"),    QStringLiteral("Cestina")},
        {QStringLiteral("da"),    QStringLiteral("Dansk")},
        {QStringLiteral("de"),    QStringLiteral("Deutsch")},
        {QStringLiteral("en"),    QStringLiteral("English")},
        {QStringLiteral("es"),    QStringLiteral("Espanol")},
        {QStringLiteral("fi"),    QStringLiteral("Suomi")},
        {QStringLiteral("fr"),    QStringLiteral("Francais")},
        {QStringLiteral("hu"),    QStringLiteral("Magyar")},
        {QStringLiteral("it"),    QStringLiteral("Italiano")},
        {QStringLiteral("ja"),    QStringLiteral("Japanese")},
        {QStringLiteral("ko"),    QStringLiteral("Korean")},
        {QStringLiteral("nb"),    QStringLiteral("Norsk bokmal")},
        {QStringLiteral("nl"),    QStringLiteral("Nederlands")},
        {QStringLiteral("pl"),    QStringLiteral("Polski")},
        {QStringLiteral("pt"),    QStringLiteral("Portugues")},
        {QStringLiteral("ro"),    QStringLiteral("Romana")},
        {QStringLiteral("ru"),    QStringLiteral("Russian")},
        {QStringLiteral("sk"),    QStringLiteral("Slovencina")},
        {QStringLiteral("sv"),    QStringLiteral("Svenska")},
        {QStringLiteral("tr"),    QStringLiteral("Turkce")},
        {QStringLiteral("zh_CN"), QStringLiteral("Chinese (Simplified)")},
        {QStringLiteral("ar"),    QStringLiteral("Arabic")},
    };
    for (const auto &e : langs)
    {
        m_languageCombo->addItem(e.display, e.code);
    }

    m_langHint = new QLabel(tr("Neustart erforderlich, um die Sprache zu wechseln."));
    Q_ASSERT(m_langHint != nullptr);
    m_langHint->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; }").arg(c.accent));
    m_langHint->setVisible(false);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
    {
        m_langHint->setVisible(true);
    });
    langForm->addRow(tr("Sprache:"), m_languageCombo);
    langForm->addRow(m_langHint);
    lay->addWidget(grpLang);
}

void SettingsDialog::setupStartupSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    const auto &c = TM().colors();
    auto *grpStart = new QGroupBox(tr("Start-Verhalten"));
    Q_ASSERT(grpStart != nullptr);
    auto *startLay = new QVBoxLayout(grpStart);
    Q_ASSERT(startLay != nullptr);
    
    m_startupGroup = new QButtonGroup(this);
    Q_ASSERT(m_startupGroup != nullptr);
    auto *rbLast = new QRadioButton(tr("Mit letzter Sitzung starten (Letzte Pfade)"));
    auto *rbDrives = new QRadioButton(tr("Immer in der Laufwerks-Übersicht (Dieser PC) starten"));
    auto *rbFixed = new QRadioButton(tr("Immer in folgendem Pfad starten:"));
    
    m_startupGroup->addButton(rbLast, 0);
    m_startupGroup->addButton(rbDrives, 1);
    m_startupGroup->addButton(rbFixed, 2);
    
    startLay->addWidget(rbLast);
    startLay->addWidget(rbDrives);
    startLay->addWidget(rbFixed);
    
    auto *pathRow = new QHBoxLayout();
    m_startupPathEdit = new QLineEdit();
    Q_ASSERT(m_startupPathEdit != nullptr);
    m_startupPathEdit->setStyleSheet(QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px; }")
                                     .arg(c.bgInput, c.borderAlt));
    auto *btnBrowse = new QPushButton(tr("Durchsuchen..."));
    Q_ASSERT(btnBrowse != nullptr);
    pathRow->addWidget(m_startupPathEdit, 1);
    pathRow->addWidget(btnBrowse);
    startLay->addLayout(pathRow);
    
    lay->addWidget(grpStart);
    
    connect(btnBrowse, &QPushButton::clicked, this, [this]()
    {
        QString p = QFileDialog::getExistingDirectory(this, tr("Start-Verzeichnis wählen"), m_startupPathEdit->text());
        if (!p.isEmpty())
        {
            m_startupPathEdit->setText(p);
        }
    });
    connect(m_startupGroup, &QButtonGroup::idClicked, this, [this, btnBrowse](int id)
    {
        m_startupPathEdit->setDisabled(id != 2);
        btnBrowse->setDisabled(id != 2);
    });
}

void SettingsDialog::setupBehaviorSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    auto *grpBehavior = new QGroupBox(tr("Verhalten & Dateiliste"));
    Q_ASSERT(grpBehavior != nullptr);
    auto *behaviorLay = new QVBoxLayout(grpBehavior);
    Q_ASSERT(behaviorLay != nullptr);
    
    m_showHidden = new QCheckBox(tr("Versteckte Dateien anzeigen"));
    m_showExtensions = new QCheckBox(tr("Dateiendungen anzeigen"));
    m_singleClick = new QCheckBox(tr("Einfachklick zum Öffnen verwenden"));
    m_showMillerIp = new QCheckBox(tr("IP-Adresse in Miller-Spalten anzeigen"));
    
    behaviorLay->addWidget(m_showHidden);
    behaviorLay->addWidget(m_showExtensions);
    behaviorLay->addWidget(m_singleClick);
    behaviorLay->addWidget(m_showMillerIp);
    lay->addWidget(grpBehavior);
}

void SettingsDialog::setupDrivesSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    auto *grpDrives = new QGroupBox(tr("Laufwerke"));
    Q_ASSERT(grpDrives != nullptr);
    auto *drivesLay = new QVBoxLayout(grpDrives);
    Q_ASSERT(drivesLay != nullptr);
    
    m_showDriveIp = new QCheckBox(tr("IP-Adresse für Netzlaufwerke anzeigen"));
    drivesLay->addWidget(m_showDriveIp);
    lay->addWidget(grpDrives);

#ifdef SC_PLUGIN_GIT
    auto *grpGit = new QGroupBox(tr("Git"));
    Q_ASSERT(grpGit != nullptr);
    auto *gitLay = new QVBoxLayout(grpGit);
    Q_ASSERT(gitLay != nullptr);

    m_gitShowSidebar = new QCheckBox(tr("Git-Box in Sidebar anzeigen"));
    gitLay->addWidget(m_gitShowSidebar);

    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(tr("Aktualisierung:")));
    m_gitRefreshMode = new QComboBox();
    Q_ASSERT(m_gitRefreshMode != nullptr);
    m_gitRefreshMode->addItem(tr("Bei Änderungen"), QStringLiteral("onchange"));
    m_gitRefreshMode->addItem(tr("Periodisch"), QStringLiteral("periodic"));
    m_gitRefreshMode->addItem(tr("Manuell"), QStringLiteral("manual"));
    modeRow->addWidget(m_gitRefreshMode, 1);
    gitLay->addLayout(modeRow);

    auto *intRow = new QHBoxLayout();
    intRow->addWidget(new QLabel(tr("Intervall (Min):")));
    m_gitRefreshInterval = new QSpinBox();
    Q_ASSERT(m_gitRefreshInterval != nullptr);
    m_gitRefreshInterval->setRange(1, 1440);
    intRow->addWidget(m_gitRefreshInterval);
    intRow->addStretch();
    gitLay->addLayout(intRow);

    connect(m_gitRefreshMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
    {
        m_gitRefreshInterval->setEnabled(m_gitRefreshMode->currentData().toString() == QStringLiteral("periodic"));
    });

    lay->addWidget(grpGit);
#endif // SC_PLUGIN_GIT
}

void SettingsDialog::setupDisplaySection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    const auto &c = TM().colors();
    auto *grpDisplay = new QGroupBox(tr("Darstellung"));
    Q_ASSERT(grpDisplay != nullptr);
    auto *displayForm = new QFormLayout(grpDisplay);
    Q_ASSERT(displayForm != nullptr);

    m_uiFontSize = new QSpinBox();
    Q_ASSERT(m_uiFontSize != nullptr);
    m_uiFontSize->setRange(8, 24);
    m_uiFontSize->setSuffix(tr(" pt"));
    m_uiFontSize->setToolTip(tr("Steuert Schriftgröße, Icon-Größe und Zeilenhöhe"));

    m_fontCombo = new QFontComboBox();
    Q_ASSERT(m_fontCombo != nullptr);
    m_fontCombo->setEditable(true);
    m_fontCombo->setToolTip(tr("App-weite Schriftart"));

    auto *fontSizePreview = new QLabel();
    Q_ASSERT(fontSizePreview != nullptr);
    fontSizePreview->setStyleSheet(QString("color:%1; font-size:11px;").arg(c.textMuted));
    fontSizePreview->setText(tr("Icon-Größe und Zeilenhöhen werden automatisch angepasst"));

    m_uiSpacing = new QSlider(Qt::Horizontal);
    Q_ASSERT(m_uiSpacing != nullptr);
    m_uiSpacing->setRange(0, 8);
    m_uiSpacing->setTickInterval(1);
    m_uiSpacing->setTickPosition(QSlider::TicksBelow);

    auto *spacingRow = new QHBoxLayout();
    m_uiSpacingLabel = new QLabel(QStringLiteral("2"));
    Q_ASSERT(m_uiSpacingLabel != nullptr);
    m_uiSpacingLabel->setFixedWidth(20);
    spacingRow->addWidget(m_uiSpacing, 1);
    spacingRow->addWidget(m_uiSpacingLabel);

    connect(m_uiSpacing, &QSlider::valueChanged, this, [this](int v)
    {
        m_uiSpacingLabel->setText(QString::number(v));
    });

    displayForm->addRow(tr("Schriftart:"), m_fontCombo);
    displayForm->addRow(tr("Schriftgröße:"), m_uiFontSize);
    displayForm->addRow(fontSizePreview);
    displayForm->addRow(tr("Abstände:"), spacingRow);
    lay->addWidget(grpDisplay);
}

void SettingsDialog::setupBlacklistSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    const auto &c = TM().colors();
    auto *grpFilter = new QGroupBox(tr("Pfad-Filter (Blacklist)"));
    Q_ASSERT(grpFilter != nullptr);
    auto *filterLay = new QVBoxLayout(grpFilter);
    Q_ASSERT(filterLay != nullptr);
    auto *hint = new QLabel(tr("Diese Verzeichnisse werden in der Sidebar und den Laufwerkslisten versteckt."));
    Q_ASSERT(hint != nullptr);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; }").arg(c.textMuted));
    filterLay->addWidget(hint);

    m_driveBlacklist = new QListWidget();
    Q_ASSERT(m_driveBlacklist != nullptr);
    m_driveBlacklist->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                                   .arg(c.bgInput, c.borderAlt));
    filterLay->addWidget(m_driveBlacklist);

    auto *row = new QHBoxLayout();
    m_blacklistEdit = new QLineEdit();
    Q_ASSERT(m_blacklistEdit != nullptr);
    m_blacklistEdit->setPlaceholderText(tr("Neuer Pfad..."));
    m_blacklistEdit->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px;")
                                   .arg(c.bgInput, c.borderAlt));
    auto *btnAdd = new QPushButton(tr("Hinzufügen"));
    Q_ASSERT(btnAdd != nullptr);
    auto *btnDel = new QPushButton(tr("Entfernen"));
    Q_ASSERT(btnDel != nullptr);
    row->addWidget(m_blacklistEdit, 1);
    row->addWidget(btnAdd);
    row->addWidget(btnDel);
    filterLay->addLayout(row);
    lay->addWidget(grpFilter);

    connect(btnAdd, &QPushButton::clicked, this, [this]()
    {
        if (!m_blacklistEdit->text().isEmpty())
        {
            m_driveBlacklist->addItem(m_blacklistEdit->text());
            m_blacklistEdit->clear();
        }
    });
    connect(btnDel, &QPushButton::clicked, this, [this]()
    {
        delete m_driveBlacklist->currentItem();
    });
}

QWidget* SettingsDialog::createAppearancePage()
{
    auto *mainWidget = new QWidget();
    auto *mainLay = new QVBoxLayout(mainWidget);
    Q_ASSERT(mainLay != nullptr);
    mainLay->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea();
    Q_ASSERT(scroll != nullptr);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget();
    auto *lay = new QVBoxLayout(content);
    Q_ASSERT(lay != nullptr);
    lay->setContentsMargins(30, 30, 30, 30);
    lay->setSpacing(20);

    const auto &c = TM().colors();

    auto *lbl = new QLabel(tr("Erscheinungsbild"));
    Q_ASSERT(lbl != nullptr);
    lbl->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(c.accent));
    lay->addWidget(lbl);

    setupThemesSection(lay);
    setupThumbnailsSection(lay);
    setupFileTypeColorsSection(lay);
    setupAgeBadgesSection(lay);

    lay->addStretch();
    scroll->setWidget(content);
    mainLay->addWidget(scroll);
    return mainWidget;
}

void SettingsDialog::setupThemesSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    const auto &c = TM().colors();
    auto *grpThemes = new QGroupBox(tr("Design & Farben"));
    Q_ASSERT(grpThemes != nullptr);
    auto *themesLay = new QVBoxLayout(grpThemes);
    Q_ASSERT(themesLay != nullptr);
    
    m_sysCheck = new QCheckBox(tr("KDE Global Theme verwenden"));
    auto *sysHint = new QLabel(tr("Übernimmt Farben und Stil des aktiven KDE Global Themes."));
    Q_ASSERT(sysHint != nullptr);
    sysHint->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; }").arg(c.textMuted));
    themesLay->addWidget(m_sysCheck);
    themesLay->addWidget(sysHint);

    m_themeBox = new QWidget();
    Q_ASSERT(m_themeBox != nullptr);
    auto *themeInnerLay = new QVBoxLayout(m_themeBox);
    Q_ASSERT(themeInnerLay != nullptr);
    themeInnerLay->setContentsMargins(0, 10, 0, 0);
    m_themeGroup = new QButtonGroup(this);
    Q_ASSERT(m_themeGroup != nullptr);

    auto *themeGrid = new QGridLayout();
    Q_ASSERT(themeGrid != nullptr);
    themeGrid->setSpacing(8);

    const auto allThemes = TM().allThemes();
    const int columns = 2;
    for (int i = 0; i < allThemes.size(); ++i)
    {
        const auto &t = allThemes.at(i);
        auto *card = new ColorCard(nullptr, t.bgMain, c.borderAlt, 6);
        Q_ASSERT(card != nullptr);
        card->setFixedHeight(38);
        auto *cardLay = new QHBoxLayout(card);
        Q_ASSERT(cardLay != nullptr);
        cardLay->setContentsMargins(8, 4, 8, 4);
        cardLay->setSpacing(6);
        auto *rb = new QRadioButton();
        Q_ASSERT(rb != nullptr);
        m_themeGroup->addButton(rb, i);
        cardLay->addWidget(rb);
        cardLay->addWidget(new QLabel(t.name), 1);

        const QList<QColor> chipCols = {t.bgMain, t.bgBox, t.accent, t.textPrimary};
        for (int j = 0; j < chipCols.size(); ++j)
        {
            auto *chip = new ColorCard(nullptr, chipCols[j], c.borderAlt, 3);
            Q_ASSERT(chip != nullptr);
            chip->setFixedSize(16, 16);
            cardLay->addWidget(chip);
        }
        themeGrid->addWidget(card, i / columns, i % columns);
    }
    themeInnerLay->addLayout(themeGrid);
    themesLay->addWidget(m_themeBox);
    lay->addWidget(grpThemes);

    connect(m_themeGroup, &QButtonGroup::idToggled, this, [this](int id, bool checked)
    {
        if (checked)
        {
            QString name = TM().allThemes().at(id).name;
            if (name == QStringLiteral("Vorlage"))
            {
                ThemeCreatorDialog dlg(this);
                if (dlg.exec() == QDialog::Accepted)
                {
                    // Re-load settings internally to see the template
                }
            }
        }
    });
}

void SettingsDialog::setupThumbnailsSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    auto *grpThumbs = new QGroupBox(tr("Vorschaubilder (Thumbnails)"));
    Q_ASSERT(grpThumbs != nullptr);
    auto *thumbLay = new QVBoxLayout(grpThumbs);
    Q_ASSERT(thumbLay != nullptr);
    m_useThumbnails = new QCheckBox(tr("Vorschaubilder anzeigen"));
    auto *sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel(tr("Maximale Dateigröße (MB):")));
    m_maxThumbSize = new QSpinBox();
    Q_ASSERT(m_maxThumbSize != nullptr);
    m_maxThumbSize->setRange(1, 4096);
    m_maxThumbSize->setSuffix(QStringLiteral(" MB"));
    sizeRow->addWidget(m_maxThumbSize);
    sizeRow->addStretch();
    thumbLay->addWidget(m_useThumbnails);
    thumbLay->addLayout(sizeRow);
    lay->addWidget(grpThumbs);
    connect(m_useThumbnails, &QCheckBox::toggled, m_maxThumbSize, &QWidget::setEnabled);
}

void SettingsDialog::setupFileTypeColorsSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    const auto &c = TM().colors();
    auto *grpExtColors = new QGroupBox(tr("Dateityp-Farben (Hervorhebung)"));
    Q_ASSERT(grpExtColors != nullptr);
    auto *extLay = new QVBoxLayout(grpExtColors);
    Q_ASSERT(extLay != nullptr);
    
    m_fileTypeColorList = new QListWidget();
    Q_ASSERT(m_fileTypeColorList != nullptr);
    m_fileTypeColorList->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                                     .arg(c.bgInput, c.borderAlt));
    extLay->addWidget(m_fileTypeColorList);
    
    auto *extInputRow = new QHBoxLayout();
    auto *extEdit = new QLineEdit();
    Q_ASSERT(extEdit != nullptr);
    extEdit->setPlaceholderText(QStringLiteral(".js, .cpp, .pdf..."));
    extEdit->setStyleSheet(QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px; }").arg(c.bgInput, c.borderAlt));
    
    const QString subBtnSs = QString(
        "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:6px; padding:6px 12px; font-size:11px; }"
        "QPushButton:hover { background:%4; border-color:%5; }"
        "QPushButton:pressed { background:%5; color:%6; }"
    ).arg(c.bgAlternate, c.textPrimary, c.borderAlt, c.bgSelect, c.accent, c.textLight);

    auto *btnPickColor = new QPushButton(tr("Farbe wählen..."));
    Q_ASSERT(btnPickColor != nullptr);
    btnPickColor->setStyleSheet(subBtnSs);
    
    auto *btnAddExt = new QPushButton(tr("Hinzufügen"));
    Q_ASSERT(btnAddExt != nullptr);
    btnAddExt->setStyleSheet(subBtnSs);
    
    auto *btnDelExt = new QPushButton(tr("Entfernen"));
    Q_ASSERT(btnDelExt != nullptr);
    btnDelExt->setStyleSheet(subBtnSs);
    
    extInputRow->addWidget(extEdit, 1);
    extInputRow->addWidget(btnPickColor);
    extInputRow->addWidget(btnAddExt);
    extInputRow->addWidget(btnDelExt);
    extLay->addLayout(extInputRow);
    lay->addWidget(grpExtColors);
    
    static QColor lastPickedColor = c.accent;
    connect(btnPickColor, &QPushButton::clicked, this, [&]()
    {
        QColor col = QColorDialog::getColor(lastPickedColor, this, tr("Farbe für Dateityp wählen"));
        if (col.isValid())
        {
            lastPickedColor = col;
        }
    });
    connect(btnAddExt, &QPushButton::clicked, this, [this, extEdit]()
    {
        QString ext = extEdit->text().trimmed();
        if (!ext.isEmpty())
        {
            if (!ext.startsWith('.'))
            {
                ext.prepend('.');
            }
            m_fileTypeColorList->addItem(QString("%1: %2").arg(ext, lastPickedColor.name()));
            extEdit->clear();
        }
    });
    connect(btnDelExt, &QPushButton::clicked, this, [this]()
    {
        delete m_fileTypeColorList->currentItem();
    });
}

void SettingsDialog::setupAgeBadgesSection(QVBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    auto *grpAge = new QGroupBox(tr("Alters-Plaketten"));
    Q_ASSERT(grpAge != nullptr);
    auto *ageLay = new QVBoxLayout(grpAge);
    Q_ASSERT(ageLay != nullptr);
    
    m_ageColors.clear();
    for (int i = 0; i < 6; ++i)
    {
        m_ageColors.append(Config::ageBadgeColor(i));
    }
    m_gradBar = new AgeBadgeGradBar(grpAge, &m_ageColors);
    Q_ASSERT(m_gradBar != nullptr);
    ageLay->addWidget(m_gradBar);

    auto *sliders = new QFormLayout();
    Q_ASSERT(sliders != nullptr);
    m_sSlider = new QSlider(Qt::Horizontal);
    Q_ASSERT(m_sSlider != nullptr);
    m_sSlider->setRange(0, 255);
    m_lSlider = new QSlider(Qt::Horizontal);
    Q_ASSERT(m_lSlider != nullptr);
    m_lSlider->setRange(0, 255);
    sliders->addRow(tr("Sättigung:"), m_sSlider);
    sliders->addRow(tr("Helligkeit:"), m_lSlider);
    ageLay->addLayout(sliders);

    m_indicatorCheck = new QCheckBox(tr("Neue Dateien hervorheben (< 2 Tage)"));
    ageLay->addWidget(m_indicatorCheck);
    lay->addWidget(grpAge);

    connect(m_sSlider, &QSlider::valueChanged, this, &SettingsDialog::updateDynamicColors);
    connect(m_lSlider, &QSlider::valueChanged, this, &SettingsDialog::updateDynamicColors);
    connect(m_sysCheck, &QCheckBox::toggled, m_themeBox, &QWidget::setDisabled);
}

QWidget* SettingsDialog::createShortcutsPage()
{
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);
    Q_ASSERT(lay != nullptr);
    lay->setContentsMargins(20, 20, 20, 20);
    
    m_shortcutsEditor = new KShortcutsEditor(page);
    Q_ASSERT(m_shortcutsEditor != nullptr);
    m_shortcutsEditor->addCollection(MW()->actionCollection());
    lay->addWidget(m_shortcutsEditor, 1);
    
    return page;
}

void SettingsDialog::updateDynamicColors()
{
    int sMapped = 40 + (m_sSlider->value() * (255 - 40) / 255);
    int lMapped = 60 + (m_lSlider->value() * (220 - 60) / 255);
    const int hues[6] = {0, 30, 80, 160, 220, 270};
    for (int i = 0; i < 6; ++i)
    {
        int s_final = (i == 5) ? sMapped / 2 : sMapped;
        if (i < m_ageColors.size())
        {
            m_ageColors[i] = QColor::fromHsl(hues[i], s_final, lMapped);
        }
    }
    if (m_gradBar != nullptr)
    {
        m_gradBar->update();
    }
}

void SettingsDialog::load()
{
    m_sysCheck->setChecked(Config::useSystemTheme());
    m_themeBox->setDisabled(Config::useSystemTheme());
    const QString curTheme = Config::selectedTheme();
    for (int i = 0; i < TM().allThemes().size(); ++i)
    {
        if (TM().allThemes().at(i).name == curTheme)
        {
            if (auto *btn = m_themeGroup->button(i))
            {
                btn->setChecked(true);
            }
            break;
        }
    }

    m_showDriveIp->setChecked(Config::showDriveIp());
    m_driveBlacklist->clear();
    m_driveBlacklist->addItems(Config::driveBlacklist());

#ifdef SC_PLUGIN_GIT
    m_gitShowSidebar->setChecked(Config::gitShowSidebar());
    const QString gitMode = Config::gitRefreshMode();
    int gitIdx = m_gitRefreshMode->findData(gitMode);
    m_gitRefreshMode->setCurrentIndex(gitIdx >= 0 ? gitIdx : 0);
    m_gitRefreshInterval->setValue(Config::gitRefreshIntervalMinutes());
    m_gitRefreshInterval->setEnabled(gitMode == QStringLiteral("periodic"));
#endif
  
    m_showMillerIp->setChecked(Config::showMillerIp());
    m_showHidden->setChecked(Config::showHiddenFiles());
    m_showExtensions->setChecked(Config::showFileExtensions());
    m_singleClick->setChecked(Config::singleClickOpen());
    if (m_uiFontSize != nullptr)
    {
        m_uiFontSize->setValue(Config::uiFontSize());
    }
    if (m_uiSpacing != nullptr)
    {
        m_uiSpacing->setValue(Config::uiSpacing());
    }
    if (m_uiSpacingLabel != nullptr)
    {
        m_uiSpacingLabel->setText(QString::number(Config::uiSpacing()));
    }

    // Schriftart
    if (m_fontCombo != nullptr)
    {
        const QString fam = Config::uiFontFamily();
        if (!fam.isEmpty())
        {
            int idx = m_fontCombo->findText(fam, Qt::MatchFixedString);
            if (idx >= 0)
            {
                m_fontCombo->setCurrentIndex(idx);
            }
            else
            {
                m_fontCombo->setCurrentFont(QFont(fam));
            }
        }
        else
        {
            m_fontCombo->setCurrentFont(qApp->font());
        }
    }

    // Sprache
    if (m_languageCombo != nullptr)
    {
        const QString lang = Config::appLanguage();
        int idx = m_languageCombo->findData(lang);
        m_languageCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        m_langHint->setVisible(false);
    }
  
    int sb = Config::startupBehavior();
    if (auto *btn = m_startupGroup->button(sb))
    {
        btn->setChecked(true);
    }
    m_startupPathEdit->setText(Config::startupPath());
    m_startupPathEdit->setDisabled(sb != 2);

    m_useThumbnails->setChecked(Config::useThumbnails());
    m_maxThumbSize->setValue(Config::maxThumbnailSize());
    m_maxThumbSize->setDisabled(!Config::useThumbnails());

    m_fileTypeColorList->clear();
    m_fileTypeColorList->addItems(Config::fileTypeColors());

    m_sSlider->setValue(Config::ageBadgeSaturation());
    m_lSlider->setValue(Config::ageBadgeLightness());
    m_indicatorCheck->setChecked(Config::showNewIndicator());
    updateDynamicColors();
}

void SettingsDialog::save()
{
    Config::setUseSystemTheme(m_sysCheck->isChecked());
    if (!m_sysCheck->isChecked())
    {
        int id = m_themeGroup->checkedId();
        if (id >= 0)
        {
            Config::setSelectedTheme(TM().allThemes().at(id).name);
        }
    }

    Config::setShowDriveIp(m_showDriveIp->isChecked());
    QStringList bl;
    for (int i = 0; i < m_driveBlacklist->count(); ++i)
    {
        bl << m_driveBlacklist->item(i)->text();
    }
    Config::setDriveBlacklist(bl);

#ifdef SC_PLUGIN_GIT
    Config::setGitShowSidebar(m_gitShowSidebar->isChecked());
    Config::setGitRefreshMode(m_gitRefreshMode->currentData().toString());
    Config::setGitRefreshIntervalMinutes(m_gitRefreshInterval->value());
#endif

    Config::setShowMillerIp(m_showMillerIp->isChecked());
    Config::setShowHiddenFiles(m_showHidden->isChecked());
    Config::setShowFileExtensions(m_showExtensions->isChecked());
    Config::setSingleClickOpen(m_singleClick->isChecked());
    if (m_uiFontSize != nullptr)
    {
        Config::setUiFontSize(m_uiFontSize->value());
    }
    if (m_uiSpacing != nullptr)
    {
        Config::setUiSpacing(m_uiSpacing->value());
    }
    if (m_fontCombo != nullptr)
    {
        const QString fam = m_fontCombo->currentFont().family();
        Config::setUiFontFamily(fam);
        QFont f = qApp->font();
        f.setFamily(fam);
        qApp->setFont(f);
        const QString fontSs = QString("* { font-family: \"%1\"; }").arg(fam);
        QString ss = qApp->styleSheet();
        static QRegularExpression re(R"(\* \{ font-family: "[^"]*"; \})");
        ss.remove(re);
        qApp->setStyleSheet(ss + fontSs);
    }
    if (m_languageCombo != nullptr)
    {
        Config::setAppLanguage(m_languageCombo->currentData().toString());
    }
  
    Config::setStartupBehavior(m_startupGroup->checkedId());
    Config::setStartupPath(m_startupPathEdit->text());

    Config::setUseThumbnails(m_useThumbnails->isChecked());
    Config::setMaxThumbnailSize(m_maxThumbSize->value());

    QStringList extCols;
    for (int i = 0; i < m_fileTypeColorList->count(); ++i)
    {
        extCols << m_fileTypeColorList->item(i)->text();
    }
    Config::setFileTypeColors(extCols);
  
    Config::setAgeBadgeSaturation(m_sSlider->value());
    Config::setAgeBadgeLightness(m_lSlider->value());
    Config::setShowNewIndicator(m_indicatorCheck->isChecked());

    if (m_shortcutsEditor != nullptr)
    {
        m_shortcutsEditor->save();
    }

    emit settingsChanged();

    if (QMessageBox::question(this, tr("Neustart erforderlich"), tr("Einige Änderungen erfordern einen Neustart. Jetzt neu starten?")) == QMessageBox::Yes)
    {
        const QString bin = QApplication::applicationFilePath();
        if (QProcess::startDetached(bin, {}))
        {
            QApplication::quit();
        }
        else
        {
            QMessageBox::warning(this, tr("Fehler"), tr("Neustart fehlgeschlagen: %1").arg(bin));
        }
    }
}
