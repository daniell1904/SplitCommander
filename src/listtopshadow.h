#pragma once
#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <QEvent>

// --- Listen Schatten-Overlay ---
// Zeichnet einen Gradient am oberen Rand einer QListWidget-Viewport (wie bei OC).
// Verwendung: new ListTopShadow(list->viewport()); list->viewport()->installEventFilter(shadow);
class ListTopShadow : public QWidget {
public:
    explicit ListTopShadow(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFixedHeight(18);
        raise();
    }
    bool eventFilter(QObject *obj, QEvent *e) override {
        if (e->type() == QEvent::Resize) {
            setGeometry(0, 0, static_cast<QWidget*>(obj)->width(), 18);
            raise();
        }
        return false;
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        QLinearGradient grad(0, 0, 0, height());
        grad.setColorAt(0.0, QColor(0, 0, 0, 100));
        grad.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), grad);
    }
};

// Hilfsfunktion: Schatten auf beliebige QListWidget-Viewport anwenden
static inline void applyListTopShadow(QWidget *viewport) {
    auto *shadow = new ListTopShadow(viewport);
    shadow->setGeometry(0, 0, viewport->width(), 18);
    viewport->installEventFilter(shadow);
}
