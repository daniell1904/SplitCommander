#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QList>
#include "thememanager.h"

class ThemeCreatorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ThemeCreatorDialog(QWidget *parent = nullptr);
    ThemeColors resultTheme() const { return m_colors; }

private:
    ThemeColors m_colors;
    QLineEdit *m_nameEdit;
    QList<QPushButton*> m_chips;
    
    void setupUI();
    void updateChips();
    void saveAndClose();
};
