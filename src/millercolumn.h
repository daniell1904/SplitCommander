#pragma once
#include <KDirLister>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

// --- MillerColumn ---
class MillerColumn : public QWidget {
  Q_OBJECT
public:
  explicit MillerColumn(QWidget *parent = nullptr);
  void populateDrives();
  void populateDir(const QString &path);
  void setActive(bool active);
  void refreshStyle();
  const QString &path() const { return m_path; }
  QListWidget *list() { return m_list; }
  void addHeaderWidget(QWidget *w);
signals:
  void entryClicked(const QString &path, MillerColumn *self);
  void activated(MillerColumn *self);
  void headerClicked(const QString &path);
  void editPathRequested();
  void teardownRequested(const QString &udi);
  void setupRequested(const QString &udi);
  void removeFromPlacesRequested(const QString &url);
  void openInLeft(const QString &path);
  void openInRight(const QString &path);
  void propertiesRequested(const QString &path);

private:
  QListWidget *m_list = nullptr;
  QPushButton *m_colLabel = nullptr;
  QHBoxLayout *m_headerLay = nullptr;
  bool         m_active    = false;
  QString m_path;
  KDirLister *m_lister = nullptr;
};
