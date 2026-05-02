#pragma once
#include <QDialog>
#include <QString>

class PropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PropertiesDialog(const QString &path, QWidget *parent = nullptr);
};
