// --- panewidget_footer.cpp --------------------------------------------------
// Footer der PaneWidget (Statusleiste mit Dateigröße, Anzahl, Vorschau).
// ---------------------------------------------------------------------------

#include "panewidget.h"
#include "mainwindow.h"
#include <QShortcut>
#include <QKeySequence>
#include "config.h"
#include "dialogutils.h"
#include "panecomponents.h"
#include "scglobal.h"
#include "thememanager.h"
#include "thumbnailmanager.h"
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KDialogJobUiDelegate>
#include <KFileItem>
#include <KFileWidget>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/EmptyTrashJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/OpenUrlJob>
#include <KIO/StoredTransferJob>
#include <KPropertiesDialog>
#include <KShortcutsDialog>
#include <KShortcutsEditor>
#include <KTerminalLauncherJob>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeDatabase>
#include <QPointer>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

// --- PaneWidget ---

void PaneWidget::buildFooter(QVBoxLayout *rootLay) {
  auto *fw = new FooterWidget(this);
  fw->onHeightChanged = [this]() {
    positionFooterPanel();
    const QString p =
        m_lastPreviewPath.isEmpty() ? currentPath() : m_lastPreviewPath;
    refreshFooter(p,
                  m_filePane->view()->selectionModel()->selectedRows().count());
  };
  m_footerBar = fw;
  m_footerCount = fw->countLbl;
  m_footerSelected = fw->selectedLbl;
  m_footerSize = fw->sizeLbl;
  m_previewIcon = fw->previewIcon;
  m_previewInfo = fw->previewInfo;
  rootLay->addWidget(fw);
}

void PaneWidget::positionFooterPanel() {
  if (!m_footerBar)
    return;
  const int h = m_footerBar->height();
  m_footerBar->setGeometry(0, height() - h, width(), h);
  m_footerBar->raise();
}

void PaneWidget::refreshFooterForDirectory(int selectedCount) {
  const int count =
      m_filePane->view()->model()->rowCount(m_filePane->view()->rootIndex());
  m_footerCount->setText(tr("%1 Elemente").arg(count));
  m_footerSize->setText(QString());
  if (m_footerSelected) {
    if (selectedCount > 0) {
      m_footerSelected->setText(tr("%1 ausgewählt").arg(selectedCount));
      m_footerSelected->show();
    } else {
      m_footerSelected->hide();
    }
  }
  if (m_previewIcon && m_footerBar) {
    const int iconSize = qBound(120, m_footerBar->height() - 40, 1024);
    m_previewIcon->setFixedSize(iconSize, iconSize);
    if (!m_lastPreviewPixmap.isNull())
      m_previewIcon->setPixmap(m_lastPreviewPixmap.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
}

void PaneWidget::refreshFooterForLocalPath(const QString &path) {
  const QUrl url = QUrl::fromLocalFile(path);
  const QFileInfo fi(path);
  if (!fi.exists())
    return;
 
  if (fi.isDir()) {
    m_footerCount->setText(tr("Ordner"));
    m_footerSize->setText(tr("…"));
    if (m_footerSelected)
      m_footerSelected->hide();
 
    auto *watcher = new QFutureWatcher<quint64>(this);
    connect(watcher, &QFutureWatcher<quint64>::finished, this,
            [this, watcher]() {
              const quint64 sz = watcher->result();
              watcher->deleteLater();
              if (!m_footerSize)
                return;
              m_footerSize->setText(
                  QString(" | %1").arg(KFormat().formatByteSize(sz)));
            });
    watcher->setFuture(QtConcurrent::run([path]() -> quint64 {
      quint64 total = 0;
      QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
      }
      return total;
    }));
  } else {
    m_footerCount->setText(fi.fileName());
    m_footerSize->setText(
        QString(" | %1").arg(KFormat().formatByteSize(fi.size())));
    if (m_footerSelected) {
      m_footerSelected->show();
    }
  }
 
  if (!m_previewIcon || !m_previewInfo)
    return;
  const int footerH = m_footerBar->height();
  int iconSize = qBound(120, footerH - 40, 1024);
  m_previewIcon->setFixedSize(iconSize, iconSize);

  QPixmap thumb = ThumbnailManager::instance().thumbnail(path, 512);
  if (!thumb.isNull()) {
    m_lastPreviewPixmap = thumb;
    m_previewIcon->setPixmap(thumb.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    m_lastPreviewPixmap = QIcon::fromTheme(KIO::iconNameForUrl(url)).pixmap(512, 512);
    m_previewIcon->setPixmap(m_lastPreviewPixmap.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ThumbnailManager::instance().requestThumbnail(path, 512);
  }
 
  QString info = QString("<table cellpadding='1' cellspacing='0' "
                         "style='color:%1;font-size:11px;'>")
                     .arg(TM().colors().textPrimary);
  auto addRow = [&info](const QString &label, const QString &val) {
    info +=
        QString(
            "<tr><td style='padding-right:20px;white-space:nowrap;'>%1</td><td "
            "style='white-space:nowrap;'>%2</td></tr>")
            .arg(label, val);
  };
  addRow(tr("Name"), fi.fileName().toHtmlEscaped());
  addRow(tr("Typ"), fi.isDir() ? tr("Ordner")
                    : fi.suffix().isEmpty()
                        ? tr("Datei")
                        : fi.suffix().toUpper() + tr("-Datei"));
  addRow(tr("Erstellt"), fi.birthTime().toString("yyyy-MM-dd  hh:mm"));
  addRow(tr("Geändert"), fi.lastModified().toString("yyyy-MM-dd  hh:mm"));
  const qint64 days = fi.lastModified().daysTo(QDateTime::currentDateTime());
  addRow(tr("Alter"), days == 0    ? tr("Heute")
                      : days == 1  ? tr("Gestern")
                      : days < 30  ? tr("%1 t").arg(days)
                      : days < 365 ? tr("%1 m").arg(days / 30)
                                   : tr("%1 j").arg(days / 365));
  if (!fi.isDir())
    addRow(tr("Größe:"), KFormat().formatByteSize(fi.size()));
 
  const QFile::Permissions p = fi.permissions();
  QString perm;
  perm += fi.isDir() ? "d" : "-";
  perm += (p & QFile::ReadOwner) ? "r" : "-";
  perm += (p & QFile::WriteOwner) ? "w" : "-";
  perm += (p & QFile::ExeOwner) ? "x" : "-";
  perm += (p & QFile::ReadGroup) ? "r" : "-";
  perm += (p & QFile::WriteGroup) ? "w" : "-";
  perm += (p & QFile::ExeGroup) ? "x" : "-";
  perm += (p & QFile::ReadOther) ? "r" : "-";
  perm += (p & QFile::WriteOther) ? "w" : "-";
  perm += (p & QFile::ExeOther) ? "x" : "-";
  addRow(tr("Attribute"), perm);
  info += "</table>";
  m_previewInfo->setText(info);
}

void PaneWidget::refreshFooterForRemotePath(const QString &path,
                                            const QUrl &url) {
  m_footerCount->setText(url.fileName().isEmpty() ? path : url.fileName());
  m_footerSize->setText(QString());
  if (!m_previewIcon || !m_previewInfo)
    return;
  const int footerH = m_footerBar->height();
  int iconSize = qBound(120, footerH - 40, 1024);
  m_previewIcon->setFixedSize(iconSize, iconSize);
  m_previewIcon->setPixmap(
      QIcon::fromTheme(KFileItem(url).isDir() ? "folder" : "text-x-generic")
          .pixmap(iconSize, iconSize));
  m_previewInfo->setText(
      QString("<b>%1</b><br>%2").arg(url.fileName(), url.scheme()));
}
 
void PaneWidget::refreshFooter(const QString &path, int selectedCount) {
  if (!m_footerCount)
    return;
  const QUrl url(path);
  const bool isLocal = url.isLocalFile() || url.scheme().isEmpty();
  m_lastPreviewPath = path;
 
  if (path == currentPath()) {
    refreshFooterForDirectory(selectedCount);
  } else if (isLocal) {
    refreshFooterForLocalPath(path);
  } else {
    refreshFooterForRemotePath(path, url);
  }
}

bool PaneWidget::eventFilter(QObject *obj, QEvent *ev) {
  return QWidget::eventFilter(obj, ev);
}

QList<QUrl> PaneWidget::selectedUrls() const {
  return m_filePane->selectedUrls();
}

