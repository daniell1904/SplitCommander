#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

class AddNetworkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddNetworkDialog(QWidget *parent = nullptr);

    [[nodiscard]] QString url() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QString iconName() const;

private:
    void buildUI(class QVBoxLayout *mainLay);
    void buildInputFields(class QFormLayout *form, const QString &inputStyle);
    void buildIconSelector(class QFormLayout *form);

    void setupConnections(class QDialogButtonBox *buttons);
    void autoDeriveNameAndIcon(const QString &text);
    void updateIcon();

    QLineEdit   *m_urlEdit = nullptr;
    QLineEdit   *m_nameEdit = nullptr;
    QComboBox   *m_iconCombo = nullptr;
    QLabel      *m_iconPreview = nullptr;
};
