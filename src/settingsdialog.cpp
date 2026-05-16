#include "settingsdialog.h"
#include "config.h"
#include "thememanager.h"
#include "mainwindow.h"
#include "themecreatordialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
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
#include <QColorDialog>
#include <QFileDialog>

#include <KShortcutsEditor>
#include <KActionCollection>

// Local helper for gradient bar
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
        const QStringList lbl = {"1 Std","1 Tag","7 Tage","1 Monat","1 Jahr",">1 Jahr"};
        QFont f = p.font(); f.setPixelSize(10); p.setFont(f);
        for (int i = 0; i < 6; ++i) {
            double pos = i / 5.0;
            int x = 2 + (int)(pos * w);
            QColor bg = (*cols)[i];
            p.setPen(bg.lightnessF() > 0.45 ? Qt::black : Qt::white);
            p.drawText(x+2, 24, lbl[i]);
        }
    }
};

// Local helper for warning-free colored cards
class ColorCard : public QWidget {
public:
    QColor bg, border;
    int radius;
    ColorCard(QWidget *p, QColor b, QColor br, int r) : QWidget(p), bg(b), border(br), radius(r) {}
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(border, 1));
        p.setBrush(bg);
        p.drawRoundedRect(rect().adjusted(1,1,-1,-1), radius, radius);
    }
};

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Einstellungen"));
  setMinimumSize(850, 750);
  resize(900, 850);
  
  setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
  
  buildUI();
  load();
}

void SettingsDialog::showPage(Page page) {
  int idx = (int)page;
  m_sidebar->setCurrentRow(idx);
  m_stack->setCurrentIndex(idx);
  show();
  raise();
  activateWindow();
}

void SettingsDialog::buildUI() {
  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  const auto &c = TM().colors();

  // SIDEBAR
  m_sidebar = new QListWidget();
  m_sidebar->setFixedWidth(220);
  m_sidebar->setStyleSheet(QString(
    "background: %1; border: none; border-right: 1px solid %2; padding: 15px;"
  ).arg(c.bgPanel, c.borderAlt));

  m_sidebar->addItem(tr("Allgemein"));
  m_sidebar->addItem(tr("Erscheinungsbild"));
  m_sidebar->addItem(tr("Kurzbefehle"));

  root->addWidget(m_sidebar);

  // STACK
  m_stack = new QStackedWidget();
  m_stack->addWidget(createGeneralPage());
  m_stack->addWidget(createAppearancePage()); 
  m_stack->addWidget(createShortcutsPage());

  root->addWidget(m_stack, 1);

  connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

  auto *rightSide = new QVBoxLayout();
  rightSide->addWidget(m_stack, 1);
  
  auto *footer = new QHBoxLayout();
  footer->setContentsMargins(20, 10, 20, 20);
  auto *btnApply = new QPushButton(tr("Übernehmen & Neustarten"));
  auto *btnClose = new QPushButton(tr("Schließen"));
  
  const QString footerBtnBase = "border-radius: 4px; padding: 4px 16px; font-size: 13px; font-weight: bold; min-height: 28px;";
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

QWidget* SettingsDialog::createGeneralPage() {
    auto *mainWidget = new QWidget();
    auto *mainLay = new QVBoxLayout(mainWidget);
    mainLay->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget();
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(30, 30, 30, 30);
    lay->setSpacing(20);

    const auto &c = TM().colors();

    auto *lbl = new QLabel(tr("Allgemeine Einstellungen"));
    lbl->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(c.accent));
    lay->addWidget(lbl);

    // 1. START-VERHALTEN
    auto *grpStart = new QGroupBox(tr("Start-Verhalten"));
    auto *startLay = new QVBoxLayout(grpStart);
    
    m_startupGroup = new QButtonGroup(this);
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
    m_startupPathEdit->setStyleSheet(QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px; }")
                                     .arg(c.bgInput, c.borderAlt));
    auto *btnBrowse = new QPushButton(tr("Durchsuchen..."));
    pathRow->addWidget(m_startupPathEdit, 1);
    pathRow->addWidget(btnBrowse);
    startLay->addLayout(pathRow);
    
    lay->addWidget(grpStart);
    
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString p = QFileDialog::getExistingDirectory(this, tr("Start-Verzeichnis wählen"), m_startupPathEdit->text());
        if (!p.isEmpty()) m_startupPathEdit->setText(p);
    });
    connect(m_startupGroup, &QButtonGroup::idClicked, this, [this, btnBrowse](int id) {
        m_startupPathEdit->setDisabled(id != 2);
        btnBrowse->setDisabled(id != 2);
    });

    // 2. VERHALTEN
    auto *grpBehavior = new QGroupBox(tr("Verhalten & Dateiliste"));
    auto *behaviorLay = new QVBoxLayout(grpBehavior);
    
    m_showHidden = new QCheckBox(tr("Versteckte Dateien anzeigen"));
    m_showExtensions = new QCheckBox(tr("Dateiendungen anzeigen"));
    m_singleClick = new QCheckBox(tr("Einfachklick zum Öffnen verwenden"));
    m_showMillerIp = new QCheckBox(tr("IP-Adresse in Miller-Spalten anzeigen"));
    
    behaviorLay->addWidget(m_showHidden);
    behaviorLay->addWidget(m_showExtensions);
    behaviorLay->addWidget(m_singleClick);
    behaviorLay->addWidget(m_showMillerIp);
    lay->addWidget(grpBehavior);

    // 3. LAUFWERKE
    auto *grpDrives = new QGroupBox(tr("Laufwerke"));
    auto *drivesLay = new QVBoxLayout(grpDrives);
    
    m_showDriveIp = new QCheckBox(tr("IP-Adresse für Netzlaufwerke anzeigen"));
    drivesLay->addWidget(m_showDriveIp);
    lay->addWidget(grpDrives);

    // 4. PFAD-FILTER
    auto *grpFilter = new QGroupBox(tr("Pfad-Filter (Blacklist)"));
    auto *filterLay = new QVBoxLayout(grpFilter);
    auto *hint = new QLabel(tr("Diese Verzeichnisse werden in der Sidebar und den Laufwerkslisten versteckt."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; }").arg(c.textMuted));
    filterLay->addWidget(hint);

    m_driveBlacklist = new QListWidget();
    m_driveBlacklist->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                                   .arg(c.bgInput, c.borderAlt));
    filterLay->addWidget(m_driveBlacklist);

    auto *row = new QHBoxLayout();
    m_blacklistEdit = new QLineEdit();
    m_blacklistEdit->setPlaceholderText(tr("Neuer Pfad..."));
    m_blacklistEdit->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px;")
                                   .arg(c.bgInput, c.borderAlt));
    auto *btnAdd = new QPushButton(tr("Hinzufügen"));
    auto *btnDel = new QPushButton(tr("Entfernen"));
    row->addWidget(m_blacklistEdit, 1);
    row->addWidget(btnAdd);
    row->addWidget(btnDel);
    filterLay->addLayout(row);
    lay->addWidget(grpFilter);

    connect(btnAdd, &QPushButton::clicked, this, [this]() {
        if (!m_blacklistEdit->text().isEmpty()) {
            m_driveBlacklist->addItem(m_blacklistEdit->text());
            m_blacklistEdit->clear();
        }
    });
    connect(btnDel, &QPushButton::clicked, this, [this]() {
        delete m_driveBlacklist->currentItem();
    });

    lay->addStretch();
    scroll->setWidget(content);
    mainLay->addWidget(scroll);
    return mainWidget;
}

QWidget* SettingsDialog::createAppearancePage() {
  auto *mainWidget = new QWidget();
  auto *mainLay = new QVBoxLayout(mainWidget);
  mainLay->setContentsMargins(0, 0, 0, 0);

  auto *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto *content = new QWidget();
  auto *lay = new QVBoxLayout(content);
  lay->setContentsMargins(30, 30, 30, 30);
  lay->setSpacing(20);

  const auto &c = TM().colors();

  auto *lbl = new QLabel(tr("Erscheinungsbild"));
  lbl->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(c.accent));
  lay->addWidget(lbl);

  // 1. THEMES
  auto *grpThemes = new QGroupBox(tr("Design & Farben"));
  auto *themesLay = new QVBoxLayout(grpThemes);
  
  m_sysCheck = new QCheckBox(tr("KDE Global Theme verwenden"));
  auto *sysHint = new QLabel(tr("Übernimmt Farben und Stil des aktiven KDE Global Themes."));
  sysHint->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; }").arg(c.textMuted));
  themesLay->addWidget(m_sysCheck);
  themesLay->addWidget(sysHint);

  m_themeBox = new QWidget();
  auto *themeInnerLay = new QVBoxLayout(m_themeBox);
  themeInnerLay->setContentsMargins(0, 10, 0, 0);
  m_themeGroup = new QButtonGroup(this);

  auto *themeScroll = new QScrollArea();
  themeScroll->setWidgetResizable(true);
  themeScroll->setFixedHeight(200);
  themeScroll->setFrameShape(QFrame::NoFrame);
  auto *themeScrollContent = new QWidget();
  auto *themeScrollLay = new QVBoxLayout(themeScrollContent);
  
  const auto allThemes = TM().allThemes();
  for (int i = 0; i < allThemes.size(); ++i) {
      const auto &t = allThemes.at(i);
      auto *card = new ColorCard(nullptr, t.bgMain, c.borderAlt, 6);
      card->setFixedHeight(50);
      auto *cardLay = new QHBoxLayout(card);
      auto *rb = new QRadioButton();
      m_themeGroup->addButton(rb, i);
      cardLay->addWidget(rb);
      cardLay->addWidget(new QLabel(t.name), 1);
      
      const QList<QColor> chipCols = {t.bgMain, t.bgBox, t.accent, t.textPrimary};
      for (int j = 0; j < chipCols.size(); ++j) {
          auto *chip = new ColorCard(nullptr, chipCols[j], c.borderAlt, 3);
          chip->setFixedSize(20, 20); 
          cardLay->addWidget(chip);
      }
      themeScrollLay->addWidget(card);
  }
  themeScrollLay->addStretch();
  themeScroll->setWidget(themeScrollContent);
  themeInnerLay->addWidget(themeScroll);
  themesLay->addWidget(m_themeBox);
  lay->addWidget(grpThemes);

  // 2. VORSCHAUBILDER (THUMBNAILS)
  auto *grpThumbs = new QGroupBox(tr("Vorschaubilder (Thumbnails)"));
  auto *thumbLay = new QVBoxLayout(grpThumbs);
  m_useThumbnails = new QCheckBox(tr("Vorschaubilder anzeigen"));
  auto *sizeRow = new QHBoxLayout();
  sizeRow->addWidget(new QLabel(tr("Maximale Dateigröße (MB):")));
  m_maxThumbSize = new QSpinBox();
  m_maxThumbSize->setRange(1, 4096);
  m_maxThumbSize->setSuffix(" MB");
  sizeRow->addWidget(m_maxThumbSize);
  sizeRow->addStretch();
  thumbLay->addWidget(m_useThumbnails);
  thumbLay->addLayout(sizeRow);
  lay->addWidget(grpThumbs);
  connect(m_useThumbnails, &QCheckBox::toggled, m_maxThumbSize, &QWidget::setEnabled);

  // 3. DATEITYP-FARBEN
  auto *grpExtColors = new QGroupBox(tr("Dateityp-Farben (Hervorhebung)"));
  auto *extLay = new QVBoxLayout(grpExtColors);
  
  m_fileTypeColorList = new QListWidget();
  m_fileTypeColorList->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                                    .arg(c.bgInput, c.borderAlt));
  extLay->addWidget(m_fileTypeColorList);
  
  auto *extInputRow = new QHBoxLayout();
  auto *extEdit = new QLineEdit();
  extEdit->setPlaceholderText(".js, .cpp, .pdf...");
  extEdit->setStyleSheet(QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 4px; padding: 4px; }").arg(c.bgInput, c.borderAlt));
  
  const QString subBtnSs = QString(
      "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:6px; padding:6px 12px; font-size:11px; }"
      "QPushButton:hover { background:%4; border-color:%5; }"
      "QPushButton:pressed { background:%5; color:%6; }"
  ).arg(c.bgAlternate, c.textPrimary, c.borderAlt, c.bgSelect, c.accent, c.textLight);

  auto *btnPickColor = new QPushButton(tr("Farbe wählen..."));
  btnPickColor->setStyleSheet(subBtnSs);
  
  auto *btnAddExt = new QPushButton(tr("Hinzufügen"));
  btnAddExt->setStyleSheet(subBtnSs);
  
  auto *btnDelExt = new QPushButton(tr("Entfernen"));
  btnDelExt->setStyleSheet(subBtnSs);
  
  extInputRow->addWidget(extEdit, 1);
  extInputRow->addWidget(btnPickColor);
  extInputRow->addWidget(btnAddExt);
  extInputRow->addWidget(btnDelExt);
  extLay->addLayout(extInputRow);
  lay->addWidget(grpExtColors);
  
  static QColor lastPickedColor = c.accent;
  connect(btnPickColor, &QPushButton::clicked, this, [&]() {
      QColor col = QColorDialog::getColor(lastPickedColor, this, tr("Farbe für Dateityp wählen"));
      if (col.isValid()) lastPickedColor = col;
  });
  connect(btnAddExt, &QPushButton::clicked, this, [this, extEdit]() {
      QString ext = extEdit->text().trimmed();
      if (!ext.isEmpty()) {
          if (!ext.startsWith(".")) ext.prepend(".");
          m_fileTypeColorList->addItem(QString("%1: %2").arg(ext, lastPickedColor.name()));
          extEdit->clear();
      }
  });
  connect(btnDelExt, &QPushButton::clicked, this, [this]() {
      delete m_fileTypeColorList->currentItem();
  });

  // Sichtbarkeit & Designer steuern
  connect(m_themeGroup, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
      if (checked) {
          QString name = TM().allThemes().at(id).name;
          if (name == "Vorlage") {
              ThemeCreatorDialog dlg(this);
              if (dlg.exec() == QDialog::Accepted) {
                  // Re-load settings to show the new theme
                  // This is tricky because the dialog won't show the new radio button until restart
                  // but we already have that behavior.
              }
          }
      }
  });

  // 5. ICON GRÖSSEN
  auto *grpIcons = new QGroupBox(tr("Icon-Größen"));
  auto *iconForm = new QFormLayout(grpIcons);
  m_sidebarIconSize = new QSpinBox(); m_sidebarIconSize->setRange(16, 64);
  m_driveIconSize = new QSpinBox();   m_driveIconSize->setRange(16, 64);
  m_listIconSize = new QSpinBox();    m_listIconSize->setRange(16, 64);
  
  iconForm->addRow(tr("Sidebar:"), m_sidebarIconSize);
  iconForm->addRow(tr("Laufwerke:"), m_driveIconSize);
  iconForm->addRow(tr("Dateilisten:"), m_listIconSize);
  lay->addWidget(grpIcons);

  // 6. AGE BADGES
  auto *grpAge = new QGroupBox(tr("Alters-Plaketten"));
  auto *ageLay = new QVBoxLayout(grpAge);
  
  m_ageColors.clear();
  for (int i = 0; i < 6; ++i) m_ageColors.append(Config::ageBadgeColor(i));
  m_gradBar = new AgeBadgeGradBar(grpAge, &m_ageColors);
  ageLay->addWidget(m_gradBar);

  auto *sliders = new QFormLayout();
  m_sSlider = new QSlider(Qt::Horizontal); m_sSlider->setRange(0, 255);
  m_lSlider = new QSlider(Qt::Horizontal); m_lSlider->setRange(0, 255);
  sliders->addRow(tr("Sättigung:"), m_sSlider);
  sliders->addRow(tr("Helligkeit:"), m_lSlider);
  ageLay->addLayout(sliders);

  m_indicatorCheck = new QCheckBox(tr("Neue Dateien hervorheben (< 2 Tage)"));
  ageLay->addWidget(m_indicatorCheck);
  lay->addWidget(grpAge);

  connect(m_sSlider, &QSlider::valueChanged, this, &SettingsDialog::updateDynamicColors);
  connect(m_lSlider, &QSlider::valueChanged, this, &SettingsDialog::updateDynamicColors);
  connect(m_sysCheck, &QCheckBox::toggled, m_themeBox, &QWidget::setDisabled);
  
  lay->addStretch();
  scroll->setWidget(content);
  mainLay->addWidget(scroll);
  return mainWidget;
}

QWidget* SettingsDialog::createShortcutsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(20, 20, 20, 20);
    
    m_shortcutsEditor = new KShortcutsEditor(page);
    m_shortcutsEditor->addCollection(MW()->actionCollection());
    lay->addWidget(m_shortcutsEditor, 1);
    
    return page;
}


void SettingsDialog::updateDynamicColors() {
    int sMapped = 40 + (m_sSlider->value() * (255 - 40) / 255);
    int lMapped = 60 + (m_lSlider->value() * (220 - 60) / 255);
    const int hues[6] = {0, 30, 80, 160, 220, 270};
    for (int i = 0; i < 6; ++i) {
        int s_final = (i == 5) ? sMapped / 2 : sMapped;
        if (i < m_ageColors.size()) m_ageColors[i] = QColor::fromHsl(hues[i], s_final, lMapped);
    }
    if (m_gradBar) m_gradBar->update();
}

void SettingsDialog::load() {
  m_sysCheck->setChecked(Config::useSystemTheme());
  m_themeBox->setDisabled(Config::useSystemTheme());
  const QString curTheme = Config::selectedTheme();
  for (int i = 0; i < TM().allThemes().size(); ++i) {
      if (TM().allThemes().at(i).name == curTheme) {
          if (auto *btn = m_themeGroup->button(i)) btn->setChecked(true);
          break;
      }
  }

  m_showDriveIp->setChecked(Config::showDriveIp());
  m_driveBlacklist->clear();
  m_driveBlacklist->addItems(Config::driveBlacklist());
  
  m_showMillerIp->setChecked(Config::showMillerIp());
  m_showHidden->setChecked(Config::showHiddenFiles());
  m_showExtensions->setChecked(Config::showFileExtensions());
  m_singleClick->setChecked(Config::singleClickOpen());
  
  int sb = Config::startupBehavior();
  if (auto *btn = m_startupGroup->button(sb)) btn->setChecked(true);
  m_startupPathEdit->setText(Config::startupPath());
  m_startupPathEdit->setDisabled(sb != 2);

  m_useThumbnails->setChecked(Config::useThumbnails());
  m_maxThumbSize->setValue(Config::maxThumbnailSize());
  m_maxThumbSize->setDisabled(!Config::useThumbnails());

  m_fileTypeColorList->clear();
  m_fileTypeColorList->addItems(Config::fileTypeColors());

  m_sidebarIconSize->setValue(Config::sidebarIconSize());
  m_driveIconSize->setValue(Config::driveIconSize());
  m_listIconSize->setValue(Config::listIconSize());

  m_sSlider->setValue(Config::ageBadgeSaturation());
  m_lSlider->setValue(Config::ageBadgeLightness());
  m_indicatorCheck->setChecked(Config::showNewIndicator());
  updateDynamicColors();
}

void SettingsDialog::save() {
  Config::setUseSystemTheme(m_sysCheck->isChecked());
  if (!m_sysCheck->isChecked()) {
      int id = m_themeGroup->checkedId();
      if (id >= 0) Config::setSelectedTheme(TM().allThemes().at(id).name);
  }

  Config::setShowDriveIp(m_showDriveIp->isChecked());
  QStringList bl;
  for(int i=0; i < m_driveBlacklist->count(); ++i) bl << m_driveBlacklist->item(i)->text();
  Config::setDriveBlacklist(bl);

  Config::setShowMillerIp(m_showMillerIp->isChecked());
  Config::setShowHiddenFiles(m_showHidden->isChecked());
  Config::setShowFileExtensions(m_showExtensions->isChecked());
  Config::setSingleClickOpen(m_singleClick->isChecked());
  
  Config::setStartupBehavior(m_startupGroup->checkedId());
  Config::setStartupPath(m_startupPathEdit->text());

  Config::setUseThumbnails(m_useThumbnails->isChecked());
  Config::setMaxThumbnailSize(m_maxThumbSize->value());

  QStringList extCols;
  for(int i=0; i < m_fileTypeColorList->count(); ++i) extCols << m_fileTypeColorList->item(i)->text();
  Config::setFileTypeColors(extCols);
  
  Config::setSidebarIconSize(m_sidebarIconSize->value());
  Config::setDriveIconSize(m_driveIconSize->value());
  Config::setListIconSize(m_listIconSize->value());

  Config::setAgeBadgeSaturation(m_sSlider->value());
  Config::setAgeBadgeLightness(m_lSlider->value());
  Config::setShowNewIndicator(m_indicatorCheck->isChecked());

  if (m_shortcutsEditor) {
      m_shortcutsEditor->save();
  }

  emit settingsChanged();
  
  if (QMessageBox::question(this, tr("Neustart erforderlich"), 
      tr("Einige Änderungen erfordern einen Neustart. Jetzt neu starten?")) == QMessageBox::Yes) {
      QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments());
      QApplication::quit();
  }
}
