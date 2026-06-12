#include "themepreviewwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>

ThemePreviewWidget::ThemePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ThemePreviewWidget::setupUI()
{
    // Haupt-Layout für dieses Widget
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);

    // 1. Titel der Vorschau
    auto *titleWrap = new QWidget(this);
    titleWrap->setObjectName("titleWrap");
    auto *titleLay = new QHBoxLayout(titleWrap);
    titleLay->setContentsMargins(0, 0, 0, 5);
    
    m_titleLabel = new QLabel(tr("Live-Vorschau (SplitCommander Mockup)"), this);
    m_titleLabel->setObjectName("previewTitleLabel");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    titleLay->addWidget(m_titleLabel);
    titleLay->addStretch();
    
    rootLay->addWidget(titleWrap);

    // 2. Mockup Container (stellt das App-Fenster dar)
    m_container = new QFrame(this);
    m_container->setObjectName("previewContainer");
    
    auto *containerLay = new QVBoxLayout(m_container);
    containerLay->setContentsMargins(0, 0, 0, 0);
    containerLay->setSpacing(0);

    // Hauptbereich (Sidebar + Panes)
    auto *middleWidget = new QWidget(m_container);
    middleWidget->setObjectName("mockMiddle");
    auto *middleLay = new QHBoxLayout(middleWidget);
    middleLay->setContentsMargins(0, 0, 0, 0);
    middleLay->setSpacing(0);

    buildSidebar(middleLay);
    buildActivePane(middleLay);
    buildInactivePane(middleLay);

    containerLay->addWidget(middleWidget, 1);

    buildFooter(containerLay);

    rootLay->addWidget(m_container, 1);
}

void ThemePreviewWidget::buildSidebar(QHBoxLayout *middleLay)
{
    // --- SEITENLEISTE ---
    m_sidebar = new QFrame(middleLay->widget());
    m_sidebar->setObjectName("mockSidebar");
    m_sidebar->setFixedWidth(190);
    
    auto *sidebarLay = new QVBoxLayout(m_sidebar);
    sidebarLay->setContentsMargins(12, 12, 12, 12);
    sidebarLay->setSpacing(8);

    m_sidebarTitle = new QLabel(tr("FAVORITEN"), m_sidebar);
    m_sidebarTitle->setObjectName("mockSidebarTitle");
    m_sidebarTitle->setStyleSheet("font-size: 9px; font-weight: bold; letter-spacing: 0.5px;");
    sidebarLay->addWidget(m_sidebarTitle);

    // Google Drive (Active/Hovered)
    m_sidebarActiveItem = new QFrame(m_sidebar);
    m_sidebarActiveItem->setObjectName("mockSidebarActiveItem");
    auto *activeItemLay = new QHBoxLayout(m_sidebarActiveItem);
    activeItemLay->setContentsMargins(8, 6, 8, 6);
    m_sidebarActiveLabel = new QLabel(tr("☁️ Google Drive"), m_sidebarActiveItem);
    m_sidebarActiveLabel->setObjectName("mockSidebarActiveLabel");
    m_sidebarActiveLabel->setStyleSheet("font-size: 11px; font-weight: bold;");
    activeItemLay->addWidget(m_sidebarActiveLabel);
    sidebarLay->addWidget(m_sidebarActiveItem);

    sidebarLay->addSpacing(5);

    buildSidebarDriveCard(sidebarLay);

    sidebarLay->addStretch();
    middleLay->addWidget(m_sidebar);
}

void ThemePreviewWidget::buildSidebarDriveCard(QVBoxLayout *sidebarLay)
{
    // Drive Card
    m_sidebarCard = new QFrame(m_sidebar);
    m_sidebarCard->setObjectName("mockSidebarCard");
    auto *cardLay = new QVBoxLayout(m_sidebarCard);
    cardLay->setContentsMargins(10, 8, 10, 8);
    cardLay->setSpacing(5);

    m_sidebarDriveLabel = new QLabel(tr("💾 SATA-SSD (/)"), m_sidebarCard);
    m_sidebarDriveLabel->setObjectName("mockSidebarDriveLabel");
    m_sidebarDriveLabel->setStyleSheet("font-size: 11px; font-weight: bold;");
    cardLay->addWidget(m_sidebarDriveLabel);

    // Speicher-Fortschrittsbalken
    m_quotaBarBg = new QFrame(m_sidebarCard);
    m_quotaBarBg->setObjectName("mockQuotaBg");
    m_quotaBarBg->setFixedHeight(8);
    
    auto *quotaBgLay = new QHBoxLayout(m_quotaBarBg);
    quotaBgLay->setContentsMargins(0, 0, 0, 0);
    quotaBgLay->setSpacing(0);

    m_quotaBarUsed = new QFrame(m_quotaBarBg);
    m_quotaBarUsed->setObjectName("mockQuotaUsed");
    m_quotaBarUsed->setFixedHeight(8);
    m_quotaBarUsed->setFixedWidth(90); // ca. 60% gefüllt
    quotaBgLay->addWidget(m_quotaBarUsed);
    quotaBgLay->addStretch();
    cardLay->addWidget(m_quotaBarBg);

    m_sidebarQuotaLabel = new QLabel(tr("240 GB frei von 512 GB"), m_sidebarCard);
    m_sidebarQuotaLabel->setObjectName("mockSidebarQuotaLabel");
    m_sidebarQuotaLabel->setStyleSheet("font-size: 9px;");
    cardLay->addWidget(m_sidebarQuotaLabel);

    sidebarLay->addWidget(m_sidebarCard);
}

void ThemePreviewWidget::buildActivePane(QHBoxLayout *middleLay)
{
    // --- DATEIANSICHTEN ---
    // Linke Pane (Aktiv & Fokussiert)
    m_activePane = new QFrame(middleLay->widget());
    m_activePane->setObjectName("mockActivePane");
    
    auto *activePaneLay = new QVBoxLayout(m_activePane);
    activePaneLay->setContentsMargins(10, 10, 10, 10);
    activePaneLay->setSpacing(5);

    m_activePaneTitle = new QLabel(tr("📁 /home/user/Dokumente"), m_activePane);
    m_activePaneTitle->setObjectName("mockActivePaneTitle");
    m_activePaneTitle->setStyleSheet("font-size: 11px; font-weight: bold;");
    activePaneLay->addWidget(m_activePaneTitle);

    m_activeFile1 = new QLabel(tr("📄 bericht_2026.pdf"), m_activePane);
    m_activeFile1->setObjectName("mockActiveFile1");
    m_activeFile1->setStyleSheet("font-size: 11px; padding: 4px;");
    activePaneLay->addWidget(m_activeFile1);

    // Ausgewählte Datei-Zeile im aktiven Panel
    m_selectedItem = new QFrame(m_activePane);
    m_selectedItem->setObjectName("mockSelectedItem");
    auto *selLay = new QHBoxLayout(m_selectedItem);
    selLay->setContentsMargins(4, 4, 4, 4);
    m_activeFile2 = new QLabel(tr("📁 SplitCommander Project"), m_selectedItem);
    m_activeFile2->setObjectName("mockActiveFile2");
    m_activeFile2->setStyleSheet("font-size: 11px; font-weight: bold;");
    selLay->addWidget(m_activeFile2);
    activePaneLay->addWidget(m_selectedItem);

    activePaneLay->addStretch();
    middleLay->addWidget(m_activePane, 1);
}

void ThemePreviewWidget::buildInactivePane(QHBoxLayout *middleLay)
{
    // Rechte Pane (Inaktiv)
    m_inactivePane = new QFrame(middleLay->widget());
    m_inactivePane->setObjectName("mockInactivePane");
    
    auto *inactivePaneLay = new QVBoxLayout(m_inactivePane);
    inactivePaneLay->setContentsMargins(10, 10, 10, 10);
    inactivePaneLay->setSpacing(5);

    m_inactivePaneTitle = new QLabel(tr("📁 /run/media/usb"), m_inactivePane);
    m_inactivePaneTitle->setObjectName("mockInactivePaneTitle");
    m_inactivePaneTitle->setStyleSheet("font-size: 11px; font-weight: bold;");
    inactivePaneLay->addWidget(m_inactivePaneTitle);

    m_inactiveFile1 = new QLabel(tr("🖼️ urlaub_foto.png"), m_inactivePane);
    m_inactiveFile1->setObjectName("mockInactiveFile1");
    m_inactiveFile1->setStyleSheet("font-size: 11px; padding: 4px;");
    inactivePaneLay->addWidget(m_inactiveFile1);

    m_inactiveFile2 = new QLabel(tr("📄 notizen.txt"), m_inactivePane);
    m_inactiveFile2->setObjectName("mockInactiveFile2");
    m_inactiveFile2->setStyleSheet("font-size: 11px; padding: 4px;");
    inactivePaneLay->addWidget(m_inactiveFile2);

    inactivePaneLay->addStretch();
    middleLay->addWidget(m_inactivePane, 1);
}

void ThemePreviewWidget::buildFooter(QVBoxLayout *containerLay)
{
    // --- FUSSZEILE / WERKZEUGLEISTE ---
    m_footer = new QFrame(containerLay->widget());
    m_footer->setObjectName("mockFooter");
    m_footer->setFixedHeight(44);
    
    auto *footerLay = new QHBoxLayout(m_footer);
    footerLay->setContentsMargins(10, 8, 10, 8);
    footerLay->setSpacing(10);

    m_pathEdit = new QFrame(m_footer);
    m_pathEdit->setObjectName("mockPathEdit");
    auto *pathLay = new QHBoxLayout(m_pathEdit);
    pathLay->setContentsMargins(8, 2, 8, 2);
    m_pathLabel = new QLabel(tr("gdrive://google18/Dokumente/"), m_pathEdit);
    m_pathLabel->setObjectName("mockPathLabel");
    m_pathLabel->setStyleSheet("font-size: 11px; font-family: monospace;");
    pathLay->addWidget(m_pathLabel);
    
    footerLay->addWidget(m_pathEdit, 1);

    m_actionButton = new QFrame(m_footer);
    m_actionButton->setObjectName("mockButton");
    auto *btnLay = new QHBoxLayout(m_actionButton);
    btnLay->setContentsMargins(10, 2, 10, 2);
    m_btnLabel = new QLabel(tr("Neuer Ordner"), m_actionButton);
    m_btnLabel->setObjectName("mockBtnLabel");
    m_btnLabel->setStyleSheet("font-size: 11px; font-weight: bold;");
    btnLay->addWidget(m_btnLabel);

    footerLay->addWidget(m_actionButton);
    containerLay->addWidget(m_footer);
}

void ThemePreviewWidget::updateColors(const ThemeColors &c)
{
    QString qss = QString(
        "#previewContainer {"
        "  background-color: %1;"
        "  border: 2px solid %2;"
        "  border-radius: 8px;"
        "}"
        "#previewTitleLabel {"
        "  color: %3;"
        "}"
    ).arg(c.bgMain, c.border, c.accent);

    qss += buildThemeSidebarQSS(c);
    qss += buildThemePanesQSS(c);
    qss += buildThemeFooterQSS(c);

    setStyleSheet(qss);
    update();
}

QString ThemePreviewWidget::buildThemeSidebarQSS(const ThemeColors &c) const
{
    return QString(
        "#mockSidebar {"
        "  background-color: %1;"
        "  border-right: 1px solid %2;"
        "  border-top-left-radius: 6px;"
        "}"
        "#mockSidebarTitle {"
        "  color: %3;"
        "}"
        "#mockSidebarActiveItem {"
        "  background-color: %4;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "}"
        "#mockSidebarActiveLabel {"
        "  color: %5;"
        "}"
        "#mockSidebarCard {"
        "  background-color: %6;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "}"
        "#mockSidebarDriveLabel {"
        "  color: %7;"
        "}"
        "#mockSidebarQuotaLabel {"
        "  color: %8;"
        "}"
        "#mockQuotaBg {"
        "  background-color: %9;"
        "  border: 1px solid %2;"
        "  border-radius: 3px;"
        "}"
        "#mockQuotaUsed {"
        "  background-color: %5;"
        "  border-radius: 2px;"
        "}"
    ).arg(c.bgPanel, c.borderAlt, c.textMuted, c.bgHover, c.textAccent,
          c.bgBox, c.textPrimary, c.textMuted, c.bgInput);
}

QString ThemePreviewWidget::buildThemePanesQSS(const ThemeColors &c) const
{
    return QString(
        "#mockActivePane {"
        "  background-color: %1;"
        "  border-right: 1px solid %2;"
        "  border-left: 2px solid %3;"
        "}"
        "#mockActivePaneTitle {"
        "  color: %4;"
        "}"
        "#mockActiveFile1 {"
        "  color: %5;"
        "}"
        "#mockInactivePane {"
        "  background-color: %6;"
        "}"
        "#mockInactivePaneTitle {"
        "  color: %7;"
        "}"
        "#mockInactiveFile1, #mockInactiveFile2 {"
        "  color: %8;"
        "}"
        "#mockSelectedItem {"
        "  background-color: %9;"
        "  border-radius: 4px;"
        "}"
        "#mockActiveFile2 {"
        "  color: %10;"
        "}"
    ).arg(c.bgList, c.separator, c.colActive, c.textAccent, c.textPrimary,
          c.bgDeep, c.textMuted, c.textInactive, c.bgSelect, c.textLight);
}

QString ThemePreviewWidget::buildThemeFooterQSS(const ThemeColors &c) const
{
    return QString(
        "#mockFooter {"
        "  background-color: %1;"
        "  border-top: 1px solid %2;"
        "  border-bottom-left-radius: 6px;"
        "  border-bottom-right-radius: 6px;"
        "}"
        "#mockPathEdit {"
        "  background-color: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "}"
        "#mockPathLabel {"
        "  color: %4;"
        "}"
        "#mockButton {"
        "  background-color: %4;"
        "  border-radius: 4px;"
        "}"
        "#mockBtnLabel {"
        "  color: %5;"
        "}"
    ).arg(c.bgPanel, c.borderAlt, c.bgInput, c.textAccent, c.textLight);
}

void ThemePreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *child = childAt(event->pos());
        if (child) {
            if (handleTextElementClick(child)) return;
            if (handleFrameElementClick(child)) return;
        }
    }
    QWidget::mousePressEvent(event);
}

bool ThemePreviewWidget::handleTextElementClick(QWidget *child)
{
    if (child == m_sidebarTitle) {
        emit colorElementClicked("textMuted");
        return true;
    } else if (child == m_activePaneTitle || child == m_pathLabel) {
        emit colorElementClicked("textAccent");
        return true;
    } else if (child == m_inactivePaneTitle) {
        emit colorElementClicked("textMuted");
        return true;
    } else if (child == m_activeFile1) {
        emit colorElementClicked("textPrimary");
        return true;
    } else if (child == m_activeFile2 || child == m_btnLabel) {
        emit colorElementClicked("textLight");
        return true;
    } else if (child == m_inactiveFile1 || child == m_inactiveFile2) {
        emit colorElementClicked("textInactive");
        return true;
    } else if (child == m_quotaBarUsed) {
        emit colorElementClicked("accent");
        return true;
    }
    return false;
}

bool ThemePreviewWidget::handleFrameElementClick(QWidget *child)
{
    QWidget *curr = child;
    while (curr && curr != this) {
        if (curr == m_selectedItem) {
            emit colorElementClicked("bgSelect");
            return true;
        } else if (curr == m_sidebarActiveItem) {
            emit colorElementClicked("bgHover");
            return true;
        } else if (curr == m_quotaBarBg) {
            emit colorElementClicked("bgInput");
            return true;
        } else if (curr == m_sidebarCard) {
            emit colorElementClicked("bgBox");
            return true;
        } else if (curr == m_actionButton) {
            emit colorElementClicked("accent");
            return true;
        } else if (curr == m_pathEdit) {
            emit colorElementClicked("bgInput");
            return true;
        } else if (curr == m_sidebar) {
            emit colorElementClicked("bgPanel");
            return true;
        } else if (curr == m_activePane) {
            emit colorElementClicked("bgList");
            return true;
        } else if (curr == m_inactivePane) {
            emit colorElementClicked("bgDeep");
            return true;
        } else if (curr == m_footer) {
            emit colorElementClicked("bgPanel");
            return true;
        } else if (curr == m_container) {
            emit colorElementClicked("bgMain");
            return true;
        }
        curr = curr->parentWidget();
    }
    return false;
}
