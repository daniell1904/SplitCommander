#include "millerarea.h"
#include "scglobal.h"
#include "thememanager.h"
#include "drivemanager.h"
#include "panewidgets.h"
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QUrl>
#include <QFileInfo>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
// --- MillerArea ---
static constexpr int FULL_COLS = 3;

MillerArea::MillerArea(QWidget *parent) : QWidget(parent) {
  setStyleSheet(TM().ssPane() + "border-top:none;");
  auto *outerLay = new QVBoxLayout(this);
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->setSpacing(0);

  m_rowWidget = new QWidget();
  m_rowWidget->setStyleSheet(TM().ssPane());
  m_rowLayout = new QHBoxLayout(m_rowWidget);
  m_rowLayout->setContentsMargins(0, 0, 0, 0);
  m_rowLayout->setSpacing(0);

  m_stripDivider = new QFrame();
  m_stripDivider->setFrameShape(QFrame::VLine);
  m_stripDivider->setStyleSheet(
      QString("background:%1;color:%1;").arg(TM().colors().colActive));
  m_stripDivider->setFixedWidth(2);
  m_stripDivider->setFrameShadow(QFrame::Sunken);
  m_stripDivider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  m_stripDivider->setVisible(false);
  m_rowLayout->addWidget(m_stripDivider);

  m_colContainer = new QWidget();
  m_colContainer->setStyleSheet(TM().ssPane());
  m_colLayout = new QHBoxLayout(m_colContainer);
  m_colLayout->setContentsMargins(0, 0, 0, 0);
  m_colLayout->setSpacing(0);
  m_rowLayout->addWidget(m_colContainer, 1);

  outerLay->addWidget(m_rowWidget, 1);
}

void MillerArea::updateVisibleColumns() {
  const int n = m_cols.size();
  const int stripCount = qMax(0, n - FULL_COLS);

  // 1. Alte Strips bereinigen
  for (auto *s : m_strips) {
    s->hide();
    m_rowLayout->removeWidget(s);
    s->deleteLater();
  }
  m_strips.clear();

  // 2. Neue Strips für ausgeblendete Spalten
  for (int i = 0; i < stripCount; ++i) {
    QUrl u(m_cols[i]->path());
    QString label = (i == 0) ? QStringLiteral("This PC") : u.fileName();
    if (label.isEmpty())
      label = u.path();
    if (label.isEmpty())
      label = m_cols[i]->path();

    auto *strip = new MillerStrip(label, m_rowWidget);
    m_rowLayout->insertWidget(i, strip);
    m_strips.append(strip);

    connect(strip, &MillerStrip::clicked, this, [this, i]() {
      emit focusRequested();
      QString targetPath = m_cols[i]->path();
      while (m_cols.size() > i + 1) {
        trimAfter(m_cols[i]);
      }
      updateVisibleColumns();
      emit headerClicked(targetPath);
    });
  }

  // Alte Trenner entfernen und neu aufbauen
  for (auto *sep : m_colSeparators) {
    sep->hide();
    m_colLayout->removeWidget(sep);
    sep->deleteLater();
  }
  m_colSeparators.clear();

  m_stripDivider->setVisible(stripCount > 0);

  // Spalten ein-/ausblenden und Trenner neu aufbauen
  for (int i = 0; i < n; ++i) {
    const bool vis = (i >= n - FULL_COLS);
    m_cols[i]->setVisible(vis);
    m_colLayout->setStretchFactor(m_cols[i], vis ? 1 : 0);

    if (vis && i > stripCount) {
      QFrame *sep = new QFrame(m_colContainer);
      sep->setFixedWidth(1);
      sep->setStyleSheet(
          QString("background:%1;border:none;").arg(TM().colors().separator));

      int layoutIdx = m_colLayout->indexOf(m_cols[i]);
      m_colLayout->insertWidget(layoutIdx, sep);
      m_colSeparators.append(sep);
    }
  }
}

void MillerArea::refreshDrives() {
  if (m_cols.isEmpty())
    return;
  // Nur den Inhalt der ersten Spalte aktualisieren, ohne die Navigation zu
  // unterbrechen
  m_cols[0]->populateDrives();
}

void MillerArea::init() {
  connect(DriveManager::instance(), &DriveManager::drivesUpdated, this, &MillerArea::refreshDrives);
  auto *col = new MillerColumn();
  col->populateDrives();
  m_colLayout->addWidget(col, 1);
  m_cols.append(col);
  m_activeCol = col;

  connect(col, &MillerColumn::entryClicked, this,
          [this, col](const QString &path, MillerColumn *src) {
            emit focusRequested();
            trimAfter(src);
            for (auto *c : m_cols)
              c->setActive(false);
            src->setActive(true);
            m_activeCol = src;
            QString p = path;
            if ((p.startsWith("gdrive:") || p.startsWith("mtp:")) &&
                !p.endsWith("/"))
              p += "/";

            // isDir aus dem Item lesen (gespeichert in UserRole+3)
            bool itemIsDir = false;
            for (int i = 0; i < col->list()->count(); ++i) {
              if (col->list()->item(i)->data(Qt::UserRole).toString() == path) {
                itemIsDir = col->list()->item(i)->data(Qt::UserRole + 3).toBool();
                break;
              }
            }
            const QUrl u = QUrl::fromUserInput(p);
            const QString sch = u.scheme();
            const bool isKioDir = (sch == "gdrive" || sch == "mtp" ||
                sch == "smb" || sch == "sftp" || sch == "ftp" || sch == "remote");

            if (isKioDir || itemIsDir) {
              appendColumn(p);
              emit pathChanged(p);
            } else {
              auto *job = new KIO::OpenUrlJob(u);
              job->setUiDelegate(KIO::createDefaultJobUiDelegate(
                  KJobUiDelegate::AutoHandlingEnabled, nullptr));
              job->start();
            }
          });
  connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
    emit focusRequested();
    for (auto *c : m_cols)
      c->setActive(false);
    src->setActive(true);
    m_activeCol = src;
  });
  connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);

  connect(col, &MillerColumn::teardownRequested, this,
          &MillerArea::teardownRequested);
  connect(col, &MillerColumn::setupRequested, this, [this](const QString &udi) {
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      return;
    connect(
        acc, &Solid::StorageAccess::setupDone, this,
        [this, acc](Solid::ErrorType, QVariant, const QString &) {
          DriveManager::instance()->refreshAll();
          if (acc->isAccessible()) {
              // Navigiere in das neu eingehängte Laufwerk
              // m_activeCol oder m_cols[0] - wir emittieren einfach einen Klick auf die erste Spalte
              if (!m_cols.isEmpty()) {
                  m_cols[0]->list()->clearSelection();
                  for(int i=0; i<m_cols[0]->list()->count(); ++i) {
                      if (m_cols[0]->list()->item(i)->data(Qt::UserRole).toString() == acc->filePath()) {
                          m_cols[0]->list()->setCurrentRow(i);
                          emit m_cols[0]->entryClicked(acc->filePath(), m_cols[0]);
                          break;
                      }
                  }
              }
          }
        },
        Qt::SingleShotConnection);
    acc->setup();
  });
  connect(col, &MillerColumn::removeFromPlacesRequested, this,
          &MillerArea::removeFromPlacesRequested);
  connect(col, &MillerColumn::openInLeft, this,
          [this](const QString &p) { emit openInLeft(p); });
  connect(col, &MillerColumn::openInRight, this,
          [this](const QString &p) { emit openInRight(p); });
  connect(col, &MillerColumn::propertiesRequested, this,
          &MillerArea::propertiesRequested);
}

void MillerArea::refresh() {
  for (auto *col : m_cols) {
    if (col->path() == "__drives__")
      col->populateDrives();
    else
      col->populateDir(col->path());
  }
}

void MillerArea::appendColumn(const QString &path) {
  auto *col = new MillerColumn();
  col->populateDir(path);
  col->setActive(true);
  m_colLayout->addWidget(col, 1);
  m_cols.append(col);

  connect(col, &MillerColumn::entryClicked, this,
          [this, col](const QString &p2, MillerColumn *src) {
            emit focusRequested();
            trimAfter(src);
            for (auto *c : m_cols)
              c->setActive(false);
            src->setActive(true);
            m_activeCol = src;

            QUrl u2 = QUrl::fromUserInput(p2);
            const QString sch2 = u2.scheme();
            const bool isKioDir =
                (sch2 == "gdrive" || sch2 == "mtp" || sch2 == "smb" ||
                 sch2 == "sftp" || sch2 == "ftp" || sch2 == "remote");

            // isDir aus UserRole+3 lesen
            bool itemIsDir = false;
            for (int i = 0; i < col->list()->count(); ++i) {
              if (col->list()->item(i)->data(Qt::UserRole).toString() == p2) {
                itemIsDir = col->list()->item(i)->data(Qt::UserRole + 3).toBool();
                break;
              }
            }

            if (isKioDir || itemIsDir) {
              QString nav = p2;
              if (isKioDir && !nav.endsWith("/"))
                nav += "/";
              appendColumn(nav);
              emit pathChanged(p2);
            } else {
              auto *job = new KIO::OpenUrlJob(u2);
              job->setUiDelegate(KIO::createDefaultJobUiDelegate(
                  KJobUiDelegate::AutoHandlingEnabled, nullptr));
              job->start();
            }
          });
  connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
    emit focusRequested();
    for (auto *c : m_cols)
      c->setActive(false);
    src->setActive(true);
    m_activeCol = src;
  });
  connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);
  connect(col, &MillerColumn::openInLeft, this,
          [this](const QString &p) { emit openInLeft(p); });
  connect(col, &MillerColumn::openInRight, this,
          [this](const QString &p) { emit openInRight(p); });
  connect(col, &MillerColumn::propertiesRequested, this,
          &MillerArea::propertiesRequested);
  updateVisibleColumns();

  // Slide-in Animation für die neue Spalte
  col->setMaximumWidth(0);
  auto *anim = new QPropertyAnimation(col, "maximumWidth");
  anim->setDuration(400);
  anim->setStartValue(0);
  anim->setEndValue(2000); // Erlaubt dem Layout, die Spalte normal zu füllen
  anim->setEasingCurve(QEasingCurve::OutQuad);
  connect(anim, &QPropertyAnimation::finished, col, [col]() {
      col->setMaximumWidth(16777215); // Zurück auf Standard (QWIDGETSIZE_MAX)
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MillerArea::trimAfter(MillerColumn *col) {
  const int idx = m_cols.indexOf(col);
  if (idx < 0)
    return;
  while (m_cols.size() > idx + 1) {
    auto *last = m_cols.takeLast();
    last->hide();
    m_colLayout->removeWidget(last);
    last->deleteLater();

    // Zugehörigen Trenner ebenfalls entfernen
    if (!m_colSeparators.isEmpty()) {
      auto *sep = m_colSeparators.takeLast();
      sep->hide();
      m_colLayout->removeWidget(sep);
      sep->deleteLater();
    }
  }
  updateVisibleColumns();
}

void MillerArea::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); }

QString MillerArea::activePath() const {
  return m_activeCol ? m_activeCol->path() : QString();
}

QList<QUrl> MillerArea::selectedUrls() const {
  return {}; // Löschen in Miller-Spalten deaktiviert
}

void MillerArea::navigateTo(const QString &path, bool clearForward) {
  (void)clearForward;
  if (path.isEmpty())
    return;

  if (path == "__drives__") {
    if (!m_cols.isEmpty()) {
      trimAfter(m_cols[0]);
      m_cols[0]->populateDrives();
      m_cols[0]->list()->clearSelection();
      m_cols[0]->setActive(true);
      m_activeCol = m_cols[0];
    }
    return;
  }

  QUrl startUrl(path);
  if (startUrl.scheme().isEmpty() && !path.isEmpty())
    startUrl = QUrl::fromUserInput(path);
  if (startUrl.isLocalFile() && !QFileInfo::exists(startUrl.toLocalFile()))
    return;

  QString drivePath;
  if (!m_cols.isEmpty()) {
    QListWidget *driveList = m_cols[0]->list();
    const QString normPath =
        startUrl.isLocalFile() ? startUrl.toLocalFile() : startUrl.toString();
    const QString targetNorm = mw_normalizePath(normPath);
    for (int i = 0; i < driveList->count(); ++i) {
      QString dp = driveList->item(i)->data(Qt::UserRole).toString();
      QString normDp = dp;
      if (normDp.startsWith("file://"))
        normDp = QUrl(normDp).toLocalFile();
      const QString dpNorm = mw_normalizePath(normDp);

      if (!dpNorm.isEmpty() &&
          (targetNorm == dpNorm || targetNorm.startsWith(dpNorm + "/"))) {
        if (dpNorm.length() >= mw_normalizePath(drivePath).length()) {
          drivePath = dp;
          driveList->setCurrentRow(i);
          m_cols[0]->setActive(true);
        }
      }
    }
    // Fallback für KIO-Protokolle falls Discovery noch nicht fertig
    if (drivePath.isEmpty()) {
      if (startUrl.scheme() == "gdrive")
        drivePath = "gdrive:/";
      else if (startUrl.scheme() == "mtp")
        drivePath = "mtp:/";
      else if (startUrl.scheme() == "remote")
        drivePath = "remote:/";
    }
    trimAfter(m_cols[0]);
  }

  QStringList segments;
  QUrl cur = startUrl;
  while (cur.isValid()) {
    const QString curStr = cur.toString();
    segments.prepend(curStr);
    if (!drivePath.isEmpty() &&
        mw_normalizePath(curStr) == mw_normalizePath(drivePath))
      break; // Stop bei Laufwerks-Wurzel

    QUrl up = cur.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    if (up == cur ||
        (up.path().isEmpty() && up.host().isEmpty() && up.scheme() != "file"))
      break;
    cur = up;
  }

  const QString targetDir = startUrl.toString();

  // Segmente überspringen die VOR dem drivePath liegen
  int startIdx = 0;
  if (!drivePath.isEmpty()) {
    for (int i = 0; i < segments.size(); ++i) {
      if (mw_normalizePath(segments[i]) == mw_normalizePath(drivePath)) {
        startIdx = i;
        break;
      }
    }
  }

  for (int i = startIdx + 1; i < segments.size(); ++i) {
    const QString seg = segments[i - 1];
    appendColumn(seg);
    if (!m_cols.isEmpty()) {
      MillerColumn *col = m_cols.last();
      const QString next = segments[i];
      for (int r = 0; r < col->list()->count(); ++r) {
        if (col->list()->item(r)->data(Qt::UserRole).toString() == next) {
          col->list()->setCurrentRow(r);
          break;
        }
      }
    }
  }

  if (m_cols.isEmpty() || m_cols.last()->path() != targetDir)
    appendColumn(targetDir);

  updateVisibleColumns();
}

void MillerArea::setFocused(bool f) {
  m_focused = f;
  setStyleSheet(TM().ssPane());
}

