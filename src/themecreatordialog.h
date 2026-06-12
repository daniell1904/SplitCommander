#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QList>
#include "thememanager.h"

class ThemePreviewWidget;

class ThemeCreatorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ThemeCreatorDialog(QWidget *parent = nullptr);
    ~ThemeCreatorDialog() override;
    ThemeColors resultTheme() const { return m_colors; }

    struct ColorField {
        QString key;
        QString label;
        QString *ref;
        QPushButton *btn = nullptr;
    };

private:
    ThemeColors m_colors;
    ThemeColors m_originalColors;
    bool m_saved = false;
    QLineEdit *m_nameEdit;
    QList<ColorField> m_fields;
    ThemePreviewWidget *m_preview = nullptr;
    
    void setupUI();
    void initLayoutsAndHeader(class QVBoxLayout *mainLay);
    void setupColorGrid(class QHBoxLayout *middleLay);
    void setupLivePreview(class QHBoxLayout *middleLay);
    void setupBottomControls(class QVBoxLayout *mainLay);
    void pickColorForKey(const QString &key);
    void saveAndClose();
};
