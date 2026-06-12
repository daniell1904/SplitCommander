#pragma once

#include <QWidget>
#include "thememanager.h"

class QLabel;
class QFrame;

class ThemePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ThemePreviewWidget(QWidget *parent = nullptr);
    ~ThemePreviewWidget() override = default;

    void updateColors(const ThemeColors &colors);

signals:
    void colorElementClicked(const QString &colorKey);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUI();
    void buildSidebar(class QHBoxLayout *middleLay);
    void buildSidebarDriveCard(class QVBoxLayout *sidebarLay);
    void buildActivePane(class QHBoxLayout *middleLay);
    void buildInactivePane(class QHBoxLayout *middleLay);
    void buildFooter(class QVBoxLayout *containerLay);

    QString buildThemeSidebarQSS(const ThemeColors &c) const;
    QString buildThemePanesQSS(const ThemeColors &c) const;
    QString buildThemeFooterQSS(const ThemeColors &c) const;

    bool handleTextElementClick(QWidget *child);
    bool handleFrameElementClick(QWidget *child);

    // Mock UI subcomponents
    QFrame *m_container = nullptr;
    QFrame *m_sidebar = nullptr;
    QFrame *m_sidebarCard = nullptr;
    QFrame *m_activePane = nullptr;
    QFrame *m_inactivePane = nullptr;
    QFrame *m_footer = nullptr;
    QFrame *m_pathEdit = nullptr;
    QFrame *m_actionButton = nullptr;
    QFrame *m_sidebarActiveItem = nullptr;
    QFrame *m_selectedItem = nullptr;
    QFrame *m_quotaBarUsed = nullptr;
    QFrame *m_quotaBarBg = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_sidebarTitle = nullptr;
    QLabel *m_sidebarActiveLabel = nullptr;
    QLabel *m_sidebarDriveLabel = nullptr;
    QLabel *m_sidebarQuotaLabel = nullptr;
    QLabel *m_activePaneTitle = nullptr;
    QLabel *m_activeFile1 = nullptr;
    QLabel *m_activeFile2 = nullptr;
    QLabel *m_inactivePaneTitle = nullptr;
    QLabel *m_inactiveFile1 = nullptr;
    QLabel *m_inactiveFile2 = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_btnLabel = nullptr;
};
