#pragma once

#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QListWidget>
#include <QVariantAnimation>
#include <QMap>

class HoverFader : public QObject
{
    Q_OBJECT
public:
    explicit HoverFader(QListWidget *view, QObject *parent = nullptr);

    [[nodiscard]] double opacity(int row) const;

protected:
    [[nodiscard]] bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void tick();

private:
    QListWidget *m_view = nullptr;
    QVariantAnimation *m_anim = nullptr;
    int m_hoveredRow = -1;
    QMap<int, double> m_opacities;
};
