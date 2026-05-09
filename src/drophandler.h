#pragma once

#include <QObject>
#include <QAbstractItemView>
#include <QUrl>
#include <QModelIndex>
#include <functional>

class DropHandler : public QObject {
    Q_OBJECT
public:
    using UrlResolver = std::function<QUrl(const QModelIndex&)>;

    explicit DropHandler(QAbstractItemView *view, UrlResolver resolver, QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    QAbstractItemView *m_view;
    UrlResolver m_resolver;
};
