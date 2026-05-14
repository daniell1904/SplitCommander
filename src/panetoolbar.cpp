#include "panetoolbar.h"
#include "thememanager.h"
#include "config.h"
#include "scglobal.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QUrl>
#include <KFormat>

PaneToolbar::PaneToolbar(QWidget *parent) : QWidget(parent) {
  setFixedHeight(SC_TOOLBAR_H);
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(TM().ssToolbar());

  auto *vlay = new QVBoxLayout(this);
  vlay->setContentsMargins(12, 10, 12, 10);
  vlay->setSpacing(6);

  auto mk = [&](const QString &icon, const QString &tip,
                auto sig) -> QToolButton * {
    auto *b = new QToolButton();
    b->setIcon(QIcon::fromTheme(icon));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(28, 28);
    b->setToolTip(tip);
    b->setStyleSheet(TM().ssActionBtn());
    connect(b, &QToolButton::clicked, this, sig);
    return b;
  };

  // Row 1: Pfad | Aktionen
  auto *r1 = new QHBoxLayout();
  r1->setContentsMargins(0, 0, 0, 0);
  r1->setSpacing(2);
  m_pathLabel = new QLabel();
  m_pathLabel->setStyleSheet(
      QString("color:%1;font-size:18px;font-weight:300;background:transparent;")
          .arg(TM().colors().textAccent));
  r1->addWidget(m_pathLabel);
  r1->addStretch(1);
  r1->addWidget(mk("view-sort-ascending", tr("Sortieren"), &PaneToolbar::sortClicked));
  m_newFolderBtn = mk("folder-new", tr("Neu"), &PaneToolbar::newFolderClicked);
  r1->addWidget(m_newFolderBtn);
  m_copyBtn = mk("edit-copy", tr("Kopieren"), &PaneToolbar::copyClicked);
  r1->addWidget(m_copyBtn);
  m_emptyTrashBtn = mk("trash-empty", tr("Papierkorb leeren"), &PaneToolbar::emptyTrashClicked);
  m_emptyTrashBtn->hide();
  r1->addWidget(m_emptyTrashBtn);
  vlay->addLayout(r1);

  // Row 2: Anzahl | Größe
  auto *r2 = new QHBoxLayout();
  r2->setContentsMargins(0, 0, 0, 0);
  r2->setSpacing(0);
  m_countLabel = new QLabel();
  m_countLabel->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().textPrimary));
  m_selectedLabel = new QLabel();
  m_selectedLabel->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().accent));
  m_selectedLabel->hide();
  m_sizeLabel = new QLabel();
  m_sizeLabel->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().textPrimary));
  m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  r2->addWidget(m_countLabel);
  r2->addWidget(m_selectedLabel);
  r2->addWidget(m_sizeLabel);
  r2->addStretch(1);
  vlay->addLayout(r2);

  // Row 3: Navigation | Ansichtsmodi
  auto *r3 = new QHBoxLayout();
  r3->setContentsMargins(0, 0, 0, 0);
  r3->setSpacing(2);
  r3->addWidget(mk("go-previous", tr("Zurück"), &PaneToolbar::backClicked));
  r3->addWidget(mk("go-next", tr("Vorwärts"), &PaneToolbar::forwardClicked));
  r3->addWidget(mk("go-up", tr("Hoch"), &PaneToolbar::upClicked));

  auto *foldersFirstBtn = new QToolButton();
  foldersFirstBtn->setIcon(QIcon::fromTheme("go-parent-folder"));
  foldersFirstBtn->setIconSize(QSize(16, 16));
  foldersFirstBtn->setFixedSize(28, 28);
  foldersFirstBtn->setCheckable(true);
  foldersFirstBtn->setChecked(true);
  foldersFirstBtn->setToolTip(tr("Ordner zuerst"));
  foldersFirstBtn->setStyleSheet(TM().ssActionBtn());
  connect(foldersFirstBtn, &QToolButton::toggled, this, &PaneToolbar::foldersFirstToggled);
  r3->addWidget(foldersFirstBtn);
  r3->addStretch(1);

  m_viewGroup = new QButtonGroup(this);
  m_viewGroup->setExclusive(true);
  connect(m_viewGroup, &QButtonGroup::idClicked, this, &PaneToolbar::viewModeChanged);

  int modeId = 0;
  for (auto &v :
       {std::pair<const char *, const char *>{"view-list-tree", "Details"},
        {"view-list-details", "Kompakt"},
        {"view-list-icons", "Symbole"}}) {
    auto *b = new QToolButton();
    b->setIcon(QIcon::fromTheme(v.first));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(28, 28);
    b->setToolTip(v.second);
    b->setStyleSheet(TM().ssActionBtn());
    b->setCheckable(true);
    if (modeId == 0)
      b->setChecked(true);
    m_viewGroup->addButton(b, modeId++);
    r3->addWidget(b);
  }
  vlay->addLayout(r3);
}

void PaneToolbar::setViewMode(int mode) {
  if (!m_viewGroup) return;
  auto *b = m_viewGroup->button(mode);
  if (b) {
    b->blockSignals(true);
    b->setChecked(true);
    b->blockSignals(false);
  }
}

void PaneToolbar::setPath(const QString &path) {
  if (!m_pathLabel) return;
  QString name;
  if (path == "__drives__" || path == "remote:/") {
    name = tr("Dieser PC");
  } else {
    QUrl url = QUrl::fromUserInput(path);
    name = url.fileName();
    if (name.isEmpty())
      name = url.isLocalFile() ? url.toLocalFile() : path;
  }
  m_pathLabel->setText(name);
  const bool isTrash = (path == "trash:/" || path.startsWith("trash:"));
  m_emptyTrashBtn->setVisible(isTrash);
  m_newFolderBtn->setVisible(!isTrash);
  m_copyBtn->setVisible(!isTrash);
}

void PaneToolbar::setCount(int count, qint64 totalBytes) {
  if (m_countLabel)
    m_countLabel->setText(tr("%1 Elemente").arg(count));
  if (m_sizeLabel) {
    KFormat format;
    m_sizeLabel->setText(" | " + format.formatByteSize(totalBytes));
  }
}

void PaneToolbar::setSelected(int count) {
  if (!m_selectedLabel) return;
  if (count > 0) {
    m_selectedLabel->setText(tr(" | %1 ausgewählt").arg(count));
    m_selectedLabel->show();
  } else {
    m_selectedLabel->hide();
  }
}
