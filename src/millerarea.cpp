#include "millerarea.h"
#include "scglobal.h"
#include "thememanager.h"
#include "drivemanager.h"
#include "panecomponents.h"
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QUrl>
#include <QFileInfo>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <QVBoxLayout>
#include <QHBoxLayout>

static constexpr int FULL_COLS = 3;

MillerArea::MillerArea(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(TM().ssPane() + QStringLiteral("border-top:none;"));
    auto *outerLay = new QVBoxLayout(this);
    Q_ASSERT(outerLay != nullptr);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    m_rowWidget = new QWidget();
    Q_ASSERT(m_rowWidget != nullptr);
    m_rowWidget->setStyleSheet(TM().ssPane());
    m_rowLayout = new QHBoxLayout(m_rowWidget);
    Q_ASSERT(m_rowLayout != nullptr);
    m_rowLayout->setContentsMargins(0, 0, 0, 0);
    m_rowLayout->setSpacing(0);

    m_stripDivider = new QFrame();
    Q_ASSERT(m_stripDivider != nullptr);
    m_stripDivider->setFrameShape(QFrame::VLine);
    m_stripDivider->setStyleSheet(
        QStringLiteral("background:%1;color:%1;").arg(TM().colors().colActive));
    m_stripDivider->setFixedWidth(2);
    m_stripDivider->setFrameShadow(QFrame::Sunken);
    m_stripDivider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_stripDivider->setVisible(false);
    m_rowLayout->addWidget(m_stripDivider);

    m_colContainer = new QWidget();
    Q_ASSERT(m_colContainer != nullptr);
    m_colContainer->setStyleSheet(TM().ssPane());
    m_colLayout = new QHBoxLayout(m_colContainer);
    Q_ASSERT(m_colLayout != nullptr);
    m_colLayout->setContentsMargins(0, 0, 0, 0);
    m_colLayout->setSpacing(0);
    m_rowLayout->addWidget(m_colContainer, 1);

    outerLay->addWidget(m_rowWidget, 1);
}

void MillerArea::setCollapsed(bool collapsed, const QString &)
{
    Q_ASSERT(m_rowWidget != nullptr);
    m_collapsed = collapsed;
    m_rowWidget->setVisible(!collapsed);
}

void MillerArea::updateVisibleColumns()
{
    Q_ASSERT(m_rowLayout != nullptr);
    Q_ASSERT(m_colLayout != nullptr);
    Q_ASSERT(m_stripDivider != nullptr);

    const int n = m_cols.size();
    const int stripCount = qMax(0, n - FULL_COLS);

    for (auto *s : m_strips)
    {
        Q_ASSERT(s != nullptr);
        s->hide();
        m_rowLayout->removeWidget(s);
        s->deleteLater();
    }
    m_strips.clear();

    for (int i = 0; i < stripCount; ++i)
    {
        Q_ASSERT(m_cols[i] != nullptr);
        QUrl u(m_cols[i]->path());
        QString label = (i == 0) ? QStringLiteral("This PC") : u.fileName();
        if (label.isEmpty())
        {
            label = u.path();
        }
        if (label.isEmpty())
        {
            label = m_cols[i]->path();
        }

        auto *strip = new MillerStrip(label, m_rowWidget);
        Q_ASSERT(strip != nullptr);
        m_rowLayout->insertWidget(i, strip);
        m_strips.append(strip);

        connect(strip, &MillerStrip::clicked, this, [this, i]()
        {
            emit focusRequested();
            Q_ASSERT(i < m_cols.size());
            QString targetPath = m_cols[i]->path();
            while (m_cols.size() > i + 1)
            {
                trimAfter(m_cols[i]);
            }
            updateVisibleColumns();
            emit headerClicked(targetPath);
        });
    }

    for (auto *sep : m_colSeparators)
    {
        Q_ASSERT(sep != nullptr);
        sep->hide();
        m_colLayout->removeWidget(sep);
        sep->deleteLater();
    }
    m_colSeparators.clear();

    m_stripDivider->setVisible(stripCount > 0);

    for (int i = 0; i < n; ++i)
    {
        Q_ASSERT(m_cols[i] != nullptr);
        const bool vis = (i >= n - FULL_COLS);
        m_cols[i]->setVisible(vis);
        m_colLayout->setStretchFactor(m_cols[i], vis ? 1 : 0);

        if (vis && i > stripCount)
        {
            QFrame *sep = new QFrame(m_colContainer);
            Q_ASSERT(sep != nullptr);
            sep->setFixedWidth(1);
            sep->setStyleSheet(
                QStringLiteral("background:%1;border:none;").arg(TM().colors().separator));

            int layoutIdx = m_colLayout->indexOf(m_cols[i]);
            m_colLayout->insertWidget(layoutIdx, sep);
            m_colSeparators.append(sep);
        }
    }
}

void MillerArea::refreshDrives()
{
    if (m_cols.isEmpty())
    {
        return;
    }
    Q_ASSERT(m_cols[0] != nullptr);
    m_cols[0]->populateDrives();
}

void MillerArea::init()
{
    auto *dm = DriveManager::instance();
    Q_ASSERT(dm != nullptr);
    connect(dm, &DriveManager::drivesUpdated, this, &MillerArea::refreshDrives);

    auto *col = new MillerColumn();
    Q_ASSERT(col != nullptr);
    col->populateDrives();
    col->setActive(true);
    
    Q_ASSERT(m_colLayout != nullptr);
    m_colLayout->addWidget(col, 1);
    m_cols.append(col);
    m_activeCol = col;

    initColumnSignals(col);
}

void MillerArea::refresh()
{
    for (auto *col : m_cols)
    {
        Q_ASSERT(col != nullptr);
        if (col->path() == QStringLiteral("__drives__"))
        {
            col->populateDrives();
        }
        else
        {
            col->populateDir(col->path());
        }
    }
}

void MillerArea::appendColumn(const QString &path)
{
    auto *col = new MillerColumn();
    Q_ASSERT(col != nullptr);
    col->populateDir(path);

    for (auto *c : m_cols)
    {
        Q_ASSERT(c != nullptr);
        c->setActive(false);
    }
    col->setActive(true);
    
    Q_ASSERT(m_colLayout != nullptr);
    m_colLayout->addWidget(col, 1);
    m_cols.append(col);

    initColumnSignals(col);
    updateVisibleColumns();

    col->setMaximumWidth(0);
    auto *anim = new QPropertyAnimation(col, "maximumWidth");
    Q_ASSERT(anim != nullptr);
    anim->setDuration(400);
    anim->setStartValue(0);
    anim->setEndValue(2000);
    anim->setEasingCurve(QEasingCurve::OutQuad);
    connect(anim, &QPropertyAnimation::finished, col, [col]()
    {
        Q_ASSERT(col != nullptr);
        col->setMaximumWidth(16777215);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MillerArea::trimAfter(MillerColumn *col)
{
    Q_ASSERT(col != nullptr);
    Q_ASSERT(m_colLayout != nullptr);
    const int idx = m_cols.indexOf(col);
    if (idx < 0)
    {
        return;
    }
    while (m_cols.size() > idx + 1)
    {
        auto *last = m_cols.takeLast();
        Q_ASSERT(last != nullptr);
        last->hide();
        m_colLayout->removeWidget(last);
        last->deleteLater();

        if (!m_colSeparators.isEmpty())
        {
            auto *sep = m_colSeparators.takeLast();
            Q_ASSERT(sep != nullptr);
            sep->hide();
            m_colLayout->removeWidget(sep);
            sep->deleteLater();
        }
    }
    updateVisibleColumns();
}

void MillerArea::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
}

QString MillerArea::activePath() const
{
    return m_activeCol ? m_activeCol->path() : QString();
}

QList<QUrl> MillerArea::selectedUrls() const
{
    return {};
}

const QList<MillerColumn*>& MillerArea::cols() const
{
    return m_cols;
}

void MillerArea::navigateTo(const QString &path, bool clearForward)
{
    Q_UNUSED(clearForward)
    if (path.isEmpty())
    {
        return;
    }

    if (path == QStringLiteral("__drives__"))
    {
        if (!m_cols.isEmpty())
        {
            trimAfter(m_cols[0]);
            m_cols[0]->populateDrives();
            Q_ASSERT(m_cols[0]->list() != nullptr);
            m_cols[0]->list()->clearSelection();
            m_cols[0]->setActive(true);
            m_activeCol = m_cols[0];
        }
        return;
    }

    QUrl startUrl(path);
    if (startUrl.scheme().isEmpty())
    {
        startUrl = QUrl::fromUserInput(path);
    }
    if (startUrl.isLocalFile() && !QFileInfo::exists(startUrl.toLocalFile()))
    {
        return;
    }

    QString drivePath;
    if (!m_cols.isEmpty())
    {
        selectAndNavigateDrive(startUrl, drivePath);
        trimAfter(m_cols[0]);
    }

    QStringList segments;
    QUrl cur = startUrl;
    while (cur.isValid())
    {
        const QString curStr = cur.toString();
        segments.prepend(curStr);
        if (!drivePath.isEmpty() && mw_normalizePath(curStr) == mw_normalizePath(drivePath))
        {
            break;
        }

        QUrl up = cur.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        if (up == cur || (up.path().isEmpty() && up.host().isEmpty() && up.scheme() != QStringLiteral("file")))
        {
            break;
        }
        cur = up;
    }

    const QString targetDir = startUrl.toString();
    int startIdx = 0;
    if (!drivePath.isEmpty())
    {
        for (int i = 0; i < segments.size(); ++i)
        {
            if (mw_normalizePath(segments[i]) == mw_normalizePath(drivePath))
            {
                startIdx = i;
                break;
            }
        }
    }

    buildAndAppendSegments(segments, startIdx, targetDir);
    updateVisibleColumns();
}

void MillerArea::setFocused(bool f)
{
    m_focused = f;
    setStyleSheet(TM().ssPane());
}

void MillerArea::initColumnSignals(MillerColumn *col)
{
    Q_ASSERT(col != nullptr);

    connect(col, &MillerColumn::entryClicked, this, [this, col](const QString &path, MillerColumn *src)
    {
        Q_ASSERT(src != nullptr);
        emit focusRequested();
        trimAfter(src);
        for (auto *c : m_cols)
        {
            Q_ASSERT(c != nullptr);
            c->setActive(false);
        }
        src->setActive(true);
        m_activeCol = src;
        
        QString p = path;
        if ((p.startsWith(QStringLiteral("gdrive:")) || p.startsWith(QStringLiteral("mtp:"))) && !p.endsWith(QLatin1Char('/')))
        {
            p += QLatin1Char('/');
        }

        bool itemIsDir = false;
        Q_ASSERT(col->list() != nullptr);
        for (int i = 0; i < col->list()->count(); ++i)
        {
            auto *item = col->list()->item(i);
            Q_ASSERT(item != nullptr);
            if (item->data(Qt::UserRole).toString() == path)
            {
                itemIsDir = item->data(Qt::UserRole + 3).toBool();
                break;
            }
        }

        const QUrl u = QUrl::fromUserInput(p);
        const QString sch = u.scheme();
        const bool isKioDir = (sch == QStringLiteral("gdrive") || sch == QStringLiteral("mtp") ||
                               sch == QStringLiteral("smb") || sch == QStringLiteral("sftp") || 
                               sch == QStringLiteral("ftp") || sch == QStringLiteral("remote"));

        if (isKioDir || itemIsDir)
        {
            appendColumn(p);
            emit pathChanged(p);
        }
        else
        {
            auto *job = new KIO::OpenUrlJob(u);
            Q_ASSERT(job != nullptr);
            job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
            job->start();
        }
    });

    connect(col, &MillerColumn::activated, this, [this](MillerColumn *src)
    {
        Q_ASSERT(src != nullptr);
        emit focusRequested();
        for (auto *c : m_cols)
        {
            Q_ASSERT(c != nullptr);
            c->setActive(false);
        }
        src->setActive(true);
        m_activeCol = src;
    });

    connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);
    
    connect(col, &MillerColumn::editPathRequested, this, [this, col]()
    {
        Q_ASSERT(col != nullptr);
        if (!m_cols.isEmpty() && m_cols.last() == col && col->path() != QStringLiteral("__drives__"))
        {
            emit editPathRequested();
        }
        else
        {
            emit headerClicked(col->path().isEmpty() ? QStringLiteral("__drives__") : col->path());
        }
    });

    connect(col, &MillerColumn::teardownRequested, this, &MillerArea::teardownRequested);
    
    connect(col, &MillerColumn::setupRequested, this, [this](const QString &udi)
    {
        Solid::Device dev(udi);
        auto *acc = dev.as<Solid::StorageAccess>();
        if (acc != nullptr)
        {
            handleDeviceSetup(acc);
        }
    });

    connect(col, &MillerColumn::removeFromPlacesRequested, this, &MillerArea::removeFromPlacesRequested);
    
    connect(col, &MillerColumn::openInLeft, this, [this](const QString &p)
    {
        emit openInLeft(p);
    });
    
    connect(col, &MillerColumn::openInRight, this, [this](const QString &p)
    {
        emit openInRight(p);
    });
    
    connect(col, &MillerColumn::propertiesRequested, this, &MillerArea::propertiesRequested);
}

void MillerArea::handleDeviceSetup(Solid::StorageAccess *acc)
{
    Q_ASSERT(acc != nullptr);
    connect(acc, &Solid::StorageAccess::setupDone, this, [this, acc](Solid::ErrorType, QVariant, const QString &)
    {
        Q_ASSERT(acc != nullptr);
        auto *dm = DriveManager::instance();
        Q_ASSERT(dm != nullptr);
        dm->refreshAll();

        if (acc->isAccessible() && !m_cols.isEmpty())
        {
            auto *col = m_cols[0];
            Q_ASSERT(col != nullptr);
            Q_ASSERT(col->list() != nullptr);
            col->list()->clearSelection();
            for (int i = 0; i < col->list()->count(); ++i)
            {
                auto *item = col->list()->item(i);
                Q_ASSERT(item != nullptr);
                if (item->data(Qt::UserRole).toString() == acc->filePath())
                {
                    col->list()->setCurrentRow(i);
                    emit col->entryClicked(acc->filePath(), col);
                    break;
                }
            }
        }
    }, Qt::SingleShotConnection);
    acc->setup();
}

void MillerArea::selectAndNavigateDrive(const QUrl &startUrl, QString &drivePath)
{
    Q_ASSERT(m_cols[0] != nullptr);
    QListWidget *driveList = m_cols[0]->list();
    Q_ASSERT(driveList != nullptr);

    const QString normPath = startUrl.isLocalFile() ? startUrl.toLocalFile() : startUrl.toString();
    const QString targetNorm = mw_normalizePath(normPath);

    for (int i = 0; i < driveList->count(); ++i)
    {
        auto *item = driveList->item(i);
        Q_ASSERT(item != nullptr);
        const QString dp = item->data(Qt::UserRole).toString();
        QString normDp = dp;
        if (normDp.startsWith(QStringLiteral("file://")))
        {
            normDp = QUrl(normDp).toLocalFile();
        }
        const QString dpNorm = mw_normalizePath(normDp);

        if (!dpNorm.isEmpty() && (targetNorm == dpNorm || targetNorm.startsWith(dpNorm + QLatin1Char('/'))))
        {
            if (dpNorm.length() >= mw_normalizePath(drivePath).length())
            {
                drivePath = dp;
                driveList->setCurrentRow(i);
                m_cols[0]->setActive(true);
            }
        }
    }

    if (drivePath.isEmpty())
    {
        const QString sch = startUrl.scheme();
        if (sch == QStringLiteral("gdrive"))
        {
            drivePath = QStringLiteral("gdrive:/");
        }
        else if (sch == QStringLiteral("mtp"))
        {
            drivePath = QStringLiteral("mtp:/");
        }
        else if (sch == QStringLiteral("remote"))
        {
            drivePath = QStringLiteral("remote:/");
        }
    }
}

void MillerArea::buildAndAppendSegments(const QStringList &segments, int startIdx, const QString &targetDir)
{
    for (int i = startIdx + 1; i < segments.size(); ++i)
    {
        const QString seg = segments[i - 1];
        appendColumn(seg);
        if (!m_cols.isEmpty())
        {
            MillerColumn *col = m_cols.last();
            Q_ASSERT(col != nullptr);
            Q_ASSERT(col->list() != nullptr);
            const QString next = segments[i];
            for (int r = 0; r < col->list()->count(); ++r)
            {
                auto *item = col->list()->item(r);
                Q_ASSERT(item != nullptr);
                if (item->data(Qt::UserRole).toString() == next)
                {
                    col->list()->setCurrentRow(r);
                    break;
                }
            }
        }
    }

    if (m_cols.isEmpty() || m_cols.last()->path() != targetDir)
    {
        appendColumn(targetDir);
    }
}
