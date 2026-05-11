#pragma once

#include <QDialog>
#include <QSlider>
#include <QWidget>
#include <QList>
#include <QColor>
#include <QPushButton>
#include <QCheckBox>

// --- AgeBadgeDialog --- (Dialog zur Konfiguration der Alters-Plaketten (Farbkodierung für Dateialter))
class AgeBadgeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AgeBadgeDialog(QWidget *parent = nullptr);


private slots:
    void applyAndSave();
    void updateDynamicColors();

private:
    QSlider      *m_sSlider       = nullptr;
    QSlider      *m_lSlider       = nullptr;
    QWidget      *m_gradBar       = nullptr;
    QPushButton  *m_applyBtn      = nullptr;
    QCheckBox    *m_indicatorCheck = nullptr;
    QList<QColor> m_ageColors;
};

