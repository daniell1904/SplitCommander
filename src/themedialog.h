#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QPainter>
#include <QPixmap>

// --- ThemeDialog --- (Dialog zum Erstellen und Bearbeiten von Themes)
class ThemeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ThemeDialog(QWidget *parent = nullptr);

private slots:
    void applyAndSave();

private:
    QCheckBox    *m_sysCheck   = nullptr;
    QGroupBox    *m_themeBox   = nullptr;
    QButtonGroup *m_themeGroup = nullptr;
    QPushButton  *m_applyBtn   = nullptr;
};

