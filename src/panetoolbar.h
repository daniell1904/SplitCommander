#pragma once
#include <QWidget>
#include <QLabel>
#include <QToolButton>
#include <QButtonGroup>

class PaneToolbar : public QWidget {
    Q_OBJECT
public:
    explicit PaneToolbar(QWidget *parent = nullptr);
    void setPath(const QString &path);
    void setCount(int count, qint64 totalBytes);
    void setSelected(int count);
    void setViewMode(int mode);
signals:
    void foldersFirstToggled(bool on);
    void backClicked();
    void forwardClicked();
    void upClicked();
    void viewModeChanged(int mode);
    void newFolderClicked();
    void deleteClicked();
    void sortClicked();
    void actionsClicked();
    void copyClicked();
    void emptyTrashClicked();
private:
    QLabel      *m_pathLabel     = nullptr;
    QLabel      *m_countLabel    = nullptr;
    QLabel      *m_selectedLabel = nullptr;
    QLabel      *m_sizeLabel     = nullptr;
    QToolButton *m_newFolderBtn  = nullptr;
    QToolButton *m_copyBtn       = nullptr;
    QToolButton *m_emptyTrashBtn = nullptr;
    QButtonGroup *m_viewGroup    = nullptr;
};
