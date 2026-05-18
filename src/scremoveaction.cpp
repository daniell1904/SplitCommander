// --- scremoveaction.cpp -----------------------------------------------------
#include "scremoveaction.h"

#include <KActionCollection>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>

SCRemoveAction::SCRemoveAction(KActionCollection *collection, QMenu *menu)
    : QAction(menu), m_collection(collection), m_menu(menu) {
    connect(this, &QAction::triggered, this, [this]() {
        if (m_action) m_action->trigger();
    });
    if (m_menu) m_menu->installEventFilter(this);
    if (auto *app = QGuiApplication::instance()) app->installEventFilter(this);
    m_shiftPressed = QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier;
    update(m_shiftPressed ? ShiftState::Pressed : ShiftState::Released);
}

SCRemoveAction::~SCRemoveAction() {
    if (m_menu) m_menu->removeEventFilter(this);
    if (auto *app = QGuiApplication::instance()) app->removeEventFilter(this);
}

bool SCRemoveAction::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Shift && !ke->isAutoRepeat()) {
            m_shiftPressed = (event->type() == QEvent::KeyPress);
            update(m_shiftPressed ? ShiftState::Pressed : ShiftState::Released);
        }
    }
    return QObject::eventFilter(obj, event);
}

void SCRemoveAction::update(ShiftState state) {
    if (!m_collection) return;
    if (state == ShiftState::Unknown) {
        state = m_shiftPressed ? ShiftState::Pressed : ShiftState::Released;
    }
    if (state == ShiftState::Pressed) {
        m_action = m_collection->action(QStringLiteral("file_delete"));
    } else {
        m_action = m_collection->action(QStringLiteral("file_trash"));
    }
    if (m_action) {
        setText(m_action->text());
        setIcon(m_action->icon());
        setEnabled(m_action->isEnabled());
        setShortcut(state == ShiftState::Pressed
                        ? QKeySequence(Qt::SHIFT | Qt::Key_Delete)
                        : QKeySequence(Qt::Key_Delete));
        if (m_menu) m_menu->update();
    }
}
