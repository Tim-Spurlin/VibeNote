// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

#include "vibenoteapplication.h"
#include <KAuthorized>
#include <KLocalizedString>

using namespace Qt::StringLiterals;

VibeNoteApplication::VibeNoteApplication(QObject *parent)
    : AbstractKirigamiApplication(parent)
{
    setupActions();
}

void VibeNoteApplication::setupActions()
{
    AbstractKirigamiApplication::setupActions();

    auto actionName = "increment_counter"_L1;
    if (KAuthorized::authorizeAction(actionName)) {
        auto action = mainCollection()->addAction(actionName, this, &VibeNoteApplication::incrementCounter);
        action->setText(i18nc("@action:inmenu", "Increment"));
        action->setIcon(QIcon::fromTheme(u"list-add-symbolic"_s));
        mainCollection()->addAction(action->objectName(), action);
        mainCollection()->setDefaultShortcut(action, Qt::CTRL | Qt::Key_I);
    }

    readSettings();
}

#include "moc_vibenoteapplication.cpp"
