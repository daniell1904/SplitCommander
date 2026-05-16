#include "themecreatordialog.h"
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
    setMinimumSize(700, 500);
    m_colors = TM().colors(); // Basis: Aktuelles Theme
    setupUI();
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

    auto *gridBox = new QGroupBox(tr("Farben anpassen"));
    gridBox->setStyleSheet(QString("QGroupBox { font-weight: bold; border: 1px solid %1; border-radius: 8px; margin-top: 15px; padding-top: 20px; }")
                           .arg(c.borderAlt));
    auto *chipGrid = new QGridLayout(gridBox);

    struct ColorField { QString label; QString *ref; };
    QList<ColorField> fields = {
        {tr("App-Hintergrund"), &m_colors.bgMain}, {tr("Dateiliste"), &m_colors.bgDeep},
        {tr("Eingabefelder"), &m_colors.bgInput}, {tr("Karten/Favoriten"), &m_colors.bgBox},
        {tr("Sidebar/Panel"), &m_colors.bgPanel}, {tr("Akzentfarbe"), &m_colors.accent},
        {tr("Akzent-Hover"), &m_colors.accentHover}, {tr("Selektion/Markierung"), &m_colors.bgSelect},
        {tr("Hover-Effekt"), &m_colors.bgHover}, {tr("Rahmen (Standard)"), &m_colors.border},
        {tr("Rahmen (Alternativ)"), &m_colors.borderAlt}, {tr("Splitter/Trenner"), &m_colors.splitter},
        {tr("Haupttext"), &m_colors.textPrimary}, {tr("Text (Kontrast/Hell)"), &m_colors.textLight},
        {tr("Text (Akzent)"), &m_colors.textAccent}, {tr("Text (Dezent)"), &m_colors.textMuted},
        {tr("Inaktive Elemente"), &m_colors.textInactive}, {tr("Horiz. Trenner"), &m_colors.separator}
    };

    int row = 0, col = 0;
    for (auto &f : fields) {
        auto *btn = new QPushButton(f.label);
        m_chips.append(btn);
        
        auto updateBtn = [btn, &f]() {
            QColor bg( *f.ref );
            QString tc = (bg.lightness() > 140) ? "black" : "white";
            btn->setStyleSheet(QString("background: %1; color: %2; border: 1px solid rgba(0,0,0,0.2); "
                                       "border-radius: 6px; padding: 12px; font-weight: bold;")
                               .arg(*f.ref, tc));
        };
        updateBtn();
        connect(btn, &QPushButton::clicked, this, [this, f, updateBtn]() {
            QColor col = QColorDialog::getColor(QColor(*f.ref), this, tr("Farbe für %1 wählen").arg(f.label));
            if (col.isValid()) {
                *f.ref = col.name();
                updateBtn();
            }
        });
        chipGrid->addWidget(btn, row, col);
        col++; if (col > 2) { col = 0; row++; }
    }
    mainLay->addWidget(gridBox);

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

void ThemeCreatorDialog::saveAndClose()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty() || name == "Vorlage" || name == "Nord") {
        QMessageBox::warning(this, tr("Ungültiger Name"), tr("Bitte gib einen eindeutigen Namen für dein Design ein."));
        return;
    }
    
    m_colors.name = name;
    if (TM().saveTheme(m_colors)) {
        QMessageBox::information(this, tr("Gespeichert"), tr("Dein Design '%1' wurde erfolgreich gespeichert!").arg(name));
        accept();
    } else {
        QMessageBox::critical(this, tr("Fehler"), tr("Konnte das Design nicht speichern."));
    }
}
