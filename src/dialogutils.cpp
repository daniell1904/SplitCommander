#include "dialogutils.h"
#include "thememanager.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>

QString DialogUtils::getText(QWidget *parent, const QString &title, const QString &label, 
                             const QString &initial, bool *ok)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    
    auto *vl = new QVBoxLayout(&dlg);
    if (!label.isEmpty()) {
        vl->addWidget(new QLabel(label, &dlg));
    }
    
    auto *edit = new QLineEdit(initial, &dlg);
    vl->addWidget(edit);
    
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vl->addWidget(box);
    
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    
    if (dlg.exec() == QDialog::Accepted) {
        if (ok) *ok = true;
        return edit->text();
    }
    
    if (ok) *ok = false;
    return QString();
}

void DialogUtils::message(QWidget *parent, const QString &title, const QString &text)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    
    auto *vl = new QVBoxLayout(&dlg);
    auto *lbl = new QLabel(text, &dlg);
    lbl->setWordWrap(true);
    vl->addWidget(lbl);
    
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    vl->addWidget(box);
    
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    dlg.exec();
}

bool DialogUtils::question(QWidget *parent, const QString &title, const QString &text)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    
    auto *vl = new QVBoxLayout(&dlg);
    auto *lbl = new QLabel(text, &dlg);
    lbl->setWordWrap(true);
    vl->addWidget(lbl);
    
    auto *box = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No, &dlg);
    vl->addWidget(box);
    
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    
    return dlg.exec() == QDialog::Accepted;
}
