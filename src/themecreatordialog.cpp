#include "themecreatordialog.h"
#include "themepreviewwidget.h"
#include "config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>
#include <QGroupBox>

ThemeCreatorDialog::ThemeCreatorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SplitCommander - Design Designer"));
    setMinimumSize(1100, 700);
    resize(1100, 700);
    m_colors = TM().colors(); // Basis: Aktuelles Theme
    m_originalColors = m_colors;
    setupUI();
}

ThemeCreatorDialog::~ThemeCreatorDialog()
{
    // Falls nicht gespeichert wurde, stellen wir das Original-Theme wieder her
    if (!m_saved) {
        TM().setTemporaryColors(m_originalColors);
    }
}

void ThemeCreatorDialog::setupUI()
{
    const auto &c = m_colors;
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(30, 30, 30, 30);
    mainLay->setSpacing(20);

    // Styling
    setStyleSheet(QString("QDialog { background: %1; color: %2; }").arg(c.bgMain, c.textPrimary));

    auto *header = new QLabel(tr("Erstelle dein persönliches Design"));
    header->setStyleSheet(QString("font-size: 20px; font-weight: bold; color: %1;").arg(c.accent));
    mainLay->addWidget(header);

    // Horizontaler Layout für die Side-by-Side-Anordnung von Anpassungen und Live-Vorschau
    auto *middleLay = new QHBoxLayout();
    middleLay->setSpacing(25);

    auto *gridBox = new QGroupBox(tr("Farben anpassen"));
    gridBox->setStyleSheet(QString("QGroupBox { font-weight: bold; border: 1px solid %1; border-radius: 8px; margin-top: 15px; padding-top: 20px; }")
                           .arg(c.borderAlt));
    auto *chipGrid = new QGridLayout(gridBox);

    m_fields = {
        {"bgMain", tr("App-Hintergrund"), &m_colors.bgMain},
        {"bgDeep", tr("Dateiliste"), &m_colors.bgDeep},
        {"bgInput", tr("Eingabefelder"), &m_colors.bgInput},
        {"bgBox", tr("Karten/Favoriten"), &m_colors.bgBox},
        {"bgPanel", tr("Sidebar/Panel"), &m_colors.bgPanel},
        {"accent", tr("Akzentfarbe"), &m_colors.accent},
        {"accentHover", tr("Akzent-Hover"), &m_colors.accentHover},
        {"bgSelect", tr("Selektion/Markierung"), &m_colors.bgSelect},
        {"bgHover", tr("Hover-Effekt"), &m_colors.bgHover},
        {"border", tr("Rahmen (Standard)"), &m_colors.border},
        {"borderAlt", tr("Rahmen (Alternativ)"), &m_colors.borderAlt},
        {"splitter", tr("Splitter/Trenner"), &m_colors.splitter},
        {"textPrimary", tr("Haupttext"), &m_colors.textPrimary},
        {"textLight", tr("Text (Kontrast/Hell)"), &m_colors.textLight},
        {"textAccent", tr("Text (Akzent)"), &m_colors.textAccent},
        {"textMuted", tr("Text (Dezent)"), &m_colors.textMuted},
        {"textInactive", tr("Inaktive Elemente"), &m_colors.textInactive},
        {"separator", tr("Horiz. Trenner"), &m_colors.separator}
    };

    int row = 0, col = 0;
    for (auto &f : m_fields) {
        f.btn = new QPushButton(f.label);
        
        auto updateBtn = [&f]() {
            QColor bg( *f.ref );
            QString tc = (bg.lightness() > 140) ? "black" : "white";
            f.btn->setStyleSheet(QString("background: %1; color: %2; border: 1px solid rgba(0,0,0,0.2); "
                                       "border-radius: 6px; padding: 12px; font-weight: bold;")
                               .arg(*f.ref, tc));
        };
        updateBtn();
        connect(f.btn, &QPushButton::clicked, this, [this, f]() {
            pickColorForKey(f.key);
        });
        chipGrid->addWidget(f.btn, row, col);
        col++; if (col > 2) { col = 0; row++; }
    }
    middleLay->addWidget(gridBox, 5); // GridBox bekommt etwas mehr Gewicht

    // Live Vorschau auf der rechten Seite
    m_preview = new ThemePreviewWidget(this);
    m_preview->updateColors(m_colors);
    middleLay->addWidget(m_preview, 4); // Vorschau daneben platzieren

    // Click-Signal der Live-Vorschau verbinden
    connect(m_preview, &ThemePreviewWidget::colorElementClicked, this, &ThemeCreatorDialog::pickColorForKey);

    mainLay->addLayout(middleLay, 1);

    auto *bottomRow = new QHBoxLayout();
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("Name deines Designs (z.B. My Dark Blue)"));
    m_nameEdit->setStyleSheet(QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 6px; padding: 10px; color: %3; }")
                               .arg(c.bgInput, c.borderAlt, c.textPrimary));
    
    auto *btnSave = new QPushButton(tr("Design speichern & schließen"));
    btnSave->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: none; border-radius: 6px; padding: 10px 25px; font-weight: bold; }"
                                   "QPushButton:hover { background: %3; }")
                           .arg(c.accent, c.textLight, c.accentHover));
    
    bottomRow->addWidget(m_nameEdit, 1);
    bottomRow->addWidget(btnSave);
    mainLay->addLayout(bottomRow);

    connect(btnSave, &QPushButton::clicked, this, &ThemeCreatorDialog::saveAndClose);
}

void ThemeCreatorDialog::pickColorForKey(const QString &key)
{
    for (auto &f : m_fields) {
        if (f.key == key) {
            QColor col = QColorDialog::getColor(QColor(*f.ref), this, tr("Farbe für %1 wählen").arg(f.label));
            if (col.isValid()) {
                *f.ref = col.name();
                
                // Button-Styling aktualisieren
                QColor bg( *f.ref );
                QString tc = (bg.lightness() > 140) ? "black" : "white";
                f.btn->setStyleSheet(QString("background: %1; color: %2; border: 1px solid rgba(0,0,0,0.2); "
                                           "border-radius: 6px; padding: 12px; font-weight: bold;")
                                   .arg(*f.ref, tc));
                
                // Vorschau aktualisieren
                if (m_preview) {
                    m_preview->updateColors(m_colors);
                }

                // --- Live-Vorschau auf die GESAMTE ANWENDUNG im Hintergrund anwenden ---
                TM().setTemporaryColors(m_colors);
            }
            break;
        }
    }
}

void ThemeCreatorDialog::saveAndClose()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty() || name == "Vorlage" || name == "Nord") {
        QMessageBox::warning(this, tr("Ungültiger Name"), tr("Bitte gib einen eindeutigen Namen für dein Design ein."));
        return;
    }
    
    m_colors.name = name;
    if (TM().saveTheme(m_colors)) {
        Config::setUseSystemTheme(false);
        Config::setSelectedTheme(name);
        m_saved = true;
        TM().apply();
        
        QMessageBox::information(this, tr("Gespeichert"), tr("Dein Design '%1' wurde erfolgreich gespeichert und angewendet!").arg(name));
        accept();
    } else {
        QMessageBox::critical(this, tr("Fehler"), tr("Konnte das Design nicht speichern."));
    }
}
