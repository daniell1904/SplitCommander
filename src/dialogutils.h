#pragma once

#include <QString>
#include <QWidget>

class DialogUtils
{
public:
    [[nodiscard]] static QString getText(QWidget *parent, const QString &title, const QString &label, 
                                         const QString &initial = QString(), bool *ok = nullptr);
    
    static void message(QWidget *parent, const QString &title, const QString &text);
    
    [[nodiscard]] static bool question(QWidget *parent, const QString &title, const QString &text);
};
