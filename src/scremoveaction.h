// --- scremoveaction.h -------------------------------------------------------
// QAction die zwischen "In den Papierkorb" und "Löschen" umschaltet
// abhängig vom Shift-Modifier (wie DolphinRemoveAction).
// ---------------------------------------------------------------------------
#pragma once
#include <QAction>
#include <QPointer>
class KActionCollection;
class QMenu;

class SCRemoveAction : public QAction {
public:
    enum class ShiftState { Unknown, Pressed, Released };

    SCRemoveAction(KActionCollection *collection, QMenu *menu);
    ~SCRemoveAction() override;

    bool eventFilter(QObject *obj, QEvent *event) override;
    void update(ShiftState state = ShiftState::Unknown);

private:
    QPointer<KActionCollection> m_collection;
    QPointer<QAction> m_action;
    QPointer<QMenu> m_menu;
    bool m_shiftPressed = false;
};
