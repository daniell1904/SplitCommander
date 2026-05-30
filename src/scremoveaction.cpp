#include "scremoveaction.h"
#include <KActionCollection>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>

SCRemoveAction::SCRemoveAction(KActionCollection *collection, QMenu *menu)
    : QAction(menu)
    , m_collection(collection)
    , m_menu(menu)
{
    Q_ASSERT(collection != nullptr);
    Q_ASSERT(menu != nullptr);

    connect(this, &QAction::triggered, this, [this]()
            {
                if (m_action != nullptr)
                {
                    m_action->trigger();
                }
            });

    m_menu->installEventFilter(this);

    auto *app = QGuiApplication::instance();
    Q_ASSERT(app != nullptr);
    app->installEventFilter(this);

    m_shiftPressed = (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) != 0;
    update(m_shiftPressed ? ShiftState::Pressed : ShiftState::Released);
}

SCRemoveAction::~SCRemoveAction()
{
    if (m_menu != nullptr)
    {
        m_menu->removeEventFilter(this);
    }
    auto *app = QGuiApplication::instance();
    if (app != nullptr)
    {
        app->removeEventFilter(this);
    }
}

bool SCRemoveAction::eventFilter(QObject *obj, QEvent *event)
{
    Q_ASSERT(obj != nullptr);
    Q_ASSERT(event != nullptr);

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        auto *ke = static_cast<QKeyEvent *>(event);
        Q_ASSERT(ke != nullptr);
        if (ke->key() == Qt::Key_Shift && !ke->isAutoRepeat())
        {
            m_shiftPressed = (event->type() == QEvent::KeyPress);
            update(m_shiftPressed ? ShiftState::Pressed : ShiftState::Released);
        }
    }
    return QObject::eventFilter(obj, event);
}

void SCRemoveAction::update(ShiftState state)
{
    if (m_collection == nullptr)
    {
        return;
    }

    ShiftState targetState = state;
    if (targetState == ShiftState::Unknown)
    {
        targetState = m_shiftPressed ? ShiftState::Pressed : ShiftState::Released;
    }

    if (targetState == ShiftState::Pressed)
    {
        m_action = m_collection->action(QStringLiteral("file_delete"));
    }
    else
    {
        m_action = m_collection->action(QStringLiteral("file_trash"));
    }

    if (m_action != nullptr)
    {
        setText(m_action->text());
        setIcon(m_action->icon());
        setEnabled(m_action->isEnabled());
        setShortcut(targetState == ShiftState::Pressed
                        ? QKeySequence(Qt::SHIFT | Qt::Key_Delete)
                        : QKeySequence(Qt::Key_Delete));
        if (m_menu != nullptr)
        {
            m_menu->update();
        }
    }
}
