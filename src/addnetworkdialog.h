#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

// Dialog zum manuellen Hinzufügen eines Netzlaufwerks
// Felder: URL, Anzeigename, Symbol
class AddNetworkDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddNetworkDialog(QWidget *parent = nullptr);

    QString url()      const;
    QString name()     const;
    QString iconName() const;

private:
    void updateIcon();

    QLineEdit   *m_urlEdit;
    QLineEdit   *m_nameEdit;
    QComboBox   *m_iconCombo;
    QLabel      *m_iconPreview;
};
