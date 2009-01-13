/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.3, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/

#include "actionmanager_p.h"
#include "mainwindow.h"
#include "actioncontainer.h"
#include "command.h"
#include "uniqueidmanager.h"

#include <coreplugin/coreconstants.h>

#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtGui/QShortcut>
#include <QtGui/QToolBar>
#include <QtGui/QMenuBar>

namespace {
    enum { warnAboutFindFailures = 0 };
}

/*!
    \class Core::ActionManager
    \mainclass

    \brief The action manager is responsible for registration of menus and
    menu items and keyboard shortcuts.

    The ActionManager is the central bookkeeper of actions and their shortcuts and layout.
    You get the only implementation of this class from the core interface
    ICore::actionManager() method, e.g.
    \code
        ExtensionSystem::PluginManager::instance()->getObject<Core::ICore>()->actionManager()
    \endcode

    The main reasons for the need of this class is to provide a central place where the user
    can specify all his keyboard shortcuts, and to provide a solution for actions that should
    behave differently in different contexts (like the copy/replace/undo/redo actions).

    All actions that are registered with the same string id (but different context lists)
    are considered to be overloads of the same command, represented by an instance
    of the ICommand class.
    The action that is visible to the user is the one returned by ICommand::action().
    If you provide yourself a user visible representation of your action you need
    to use ICommand::action() for this.
    When this action is invoked by the user,
    the signal is forwarded to the registered action that is valid for the current context.

    So to register a globally active action "My Action"
    put the following in your plugin's IPlugin::initialize method:
    \code
        Core::ActionManager *am = ExtensionSystem::PluginManager::instance()
            ->getObject<Core::ICore>()->actionManager();
        QAction *myAction = new QAction(tr("My Action"), this);
        Core::ICommand *cmd = am->registerAction(myAction,
                                                 "myplugin.myaction",
                                                 QList<int>() << C_GLOBAL_ID);
        cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+u")));
        connect(myAction, SIGNAL(triggered()), this, SLOT(performMyAction()));
    \endcode

    So the \c connect is done to your own QAction instance. If you create e.g.
    a tool button that should represent the action you add the action
    from ICommand::action() to it:
    \code
        QToolButton *myButton = new QToolButton(someParentWidget);
        myButton->setDefaultAction(cmd->action());
    \endcode

    Also use the ActionManager to add items to registered
    action containers like the applications menu bar or menus in that menu bar.
    To do this, you register your action via the
    registerAction methods, get the action container for a specific id (like specified in
    the Core::Constants namespace) with a call of
    actionContainer(const QString&) and add your command to this container.

    Following the example adding "My Action" to the "Tools" menu would be done by
    \code
        am->actionContainer(Core::M_TOOLS)->addAction(cmd);
    \endcode

    Important guidelines:
    \list
    \o Always register your actions and shortcuts!
    \o Register your actions and shortcuts during your plugin's IPlugin::initialize
       or IPlugin::extensionsInitialized methods, otherwise the shortcuts won't appear
       in the keyboard settings dialog from the beginning.
    \o When registering an action with cmd=registerAction(action, id, contexts) be sure to connect
       your own action connect(action, SIGNAL...) but make cmd->action() visible to the user, i.e.
       widget->addAction(cmd->action()).
    \o Use this class to add actions to the applications menus
    \endlist

    \sa Core::ICore
    \sa Core::ICommand
    \sa Core::IActionContainer
    \sa Core::IContext
*/

/*!
    \fn IActionContainer *ActionManager::createMenu(const QString &id)
    \brief Creates a new menu with the given string \a id.

    Returns a new IActionContainer that you can use to get the QMenu instance
    or to add menu items to the menu. The ActionManager owns
    the returned IActionContainer.
    Add your menu to some other menu or a menu bar via the
    ActionManager::actionContainer and IActionContainer::addMenu methods.
*/

/*!
    \fn IActionContainer *ActionManager::createMenuBar(const QString &id)
    \brief Creates a new menu bar with the given string \a id.

    Returns a new IActionContainer that you can use to get the QMenuBar instance
    or to add menus to the menu bar. The ActionManager owns
    the returned IActionContainer.
*/

/*!
    \fn ICommand *ActionManager::registerAction(QAction *action, const QString &id, const QList<int> &context)
    \brief Makes an \a action known to the system under the specified string \a id.

    Returns a command object that represents the action in the application and is
    owned by the ActionManager. You can registered several actions with the
    same \a id as long as the \a context is different. In this case
    a trigger of the actual action is forwarded to the registered QAction
    for the currently active context.
*/

/*!
    \fn ICommand *ActionManager::registerShortcut(QShortcut *shortcut, const QString &id, const QList<int> &context)
    \brief Makes a \a shortcut known to the system under the specified string \a id.

    Returns a command object that represents the shortcut in the application and is
    owned by the ActionManager. You can registered several shortcuts with the
    same \a id as long as the \a context is different. In this case
    a trigger of the actual shortcut is forwarded to the registered QShortcut
    for the currently active context.
*/

/*!
    \fn ICommand *ActionManager::command(const QString &id) const
    \brief Returns the ICommand object that is known to the system
    under the given string \a id.

    \sa ActionManager::registerAction()
*/

/*!
    \fn IActionContainer *ActionManager::actionContainer(const QString &id) const
    \brief Returns the IActionContainter object that is know to the system
    under the given string \a id.

    \sa ActionManager::createMenu()
    \sa ActionManager::createMenuBar()
*/
/*!
    \fn ActionManager::ActionManager(QObject *parent)
    \internal
*/
/*!
    \fn ActionManager::~ActionManager()
    \internal
*/

using namespace Core;
using namespace Core::Internal;

ActionManagerPrivate* ActionManagerPrivate::m_instance = 0;

/*!
    \class ActionManagerPrivate
    \inheaderfile actionmanager_p.h
    \internal
*/

ActionManagerPrivate::ActionManagerPrivate(MainWindow *mainWnd, UniqueIDManager *uidmgr) :
    ActionManager(mainWnd),
    m_mainWnd(mainWnd)
{
    m_defaultGroups << uidmgr->uniqueIdentifier(Constants::G_DEFAULT_ONE);
    m_defaultGroups << uidmgr->uniqueIdentifier(Constants::G_DEFAULT_TWO);
    m_defaultGroups << uidmgr->uniqueIdentifier(Constants::G_DEFAULT_THREE);
    m_instance = this;

}

ActionManagerPrivate::~ActionManagerPrivate()
{
    qDeleteAll(m_idCmdMap.values());
    qDeleteAll(m_idContainerMap.values());
}

ActionManagerPrivate* ActionManagerPrivate::instance()
{
    return m_instance;
}

QList<int> ActionManagerPrivate::defaultGroups() const
{
    return m_defaultGroups;
}

QList<Command *> ActionManagerPrivate::commands() const
{
    return m_idCmdMap.values();
}

QList<ActionContainer *> ActionManagerPrivate::containers() const
{
    return m_idContainerMap.values();
}

bool ActionManagerPrivate::hasContext(int context) const
{
    return m_context.contains(context);
}

void ActionManagerPrivate::setContext(const QList<int> &context)
{
    // here are possibilities for speed optimization if necessary:
    // let commands (de-)register themselves for contexts
    // and only update commands that are either in old or new contexts
    m_context = context;
    const IdCmdMap::const_iterator cmdcend = m_idCmdMap.constEnd();
    for (IdCmdMap::const_iterator it = m_idCmdMap.constBegin(); it != cmdcend; ++it)
        it.value()->setCurrentContext(m_context);

    const IdContainerMap::const_iterator acend = m_idContainerMap.constEnd();
    for ( IdContainerMap::const_iterator it = m_idContainerMap.constBegin(); it != acend; ++it)
        it.value()->update();
}

bool ActionManagerPrivate::hasContext(QList<int> context) const
{
    for (int i=0; i<m_context.count(); ++i) {
        if (context.contains(m_context.at(i)))
            return true;
    }
    return false;
}

IActionContainer *ActionManagerPrivate::createMenu(const QString &id)
{
    const int uid = m_mainWnd->uniqueIDManager()->uniqueIdentifier(id);
    const IdContainerMap::const_iterator it = m_idContainerMap.constFind(uid);
    if (it !=  m_idContainerMap.constEnd())
        return it.value();

    QMenu *m = new QMenu(m_mainWnd);
    m->setObjectName(id);

    MenuActionContainer *mc = new MenuActionContainer(uid);
    mc->setMenu(m);

    m_idContainerMap.insert(uid, mc);

    return mc;
}

IActionContainer *ActionManagerPrivate::createMenuBar(const QString &id)
{
    const int uid = m_mainWnd->uniqueIDManager()->uniqueIdentifier(id);
    const IdContainerMap::const_iterator it = m_idContainerMap.constFind(uid);
    if (it !=  m_idContainerMap.constEnd())
        return it.value();

    QMenuBar *mb = new QMenuBar; // No parent (System menu bar on Mac OS X)
    mb->setObjectName(id);

    MenuBarActionContainer *mbc = new MenuBarActionContainer(uid);
    mbc->setMenuBar(mb);

    m_idContainerMap.insert(uid, mbc);

    return mbc;
}

ICommand *ActionManagerPrivate::registerAction(QAction *action, const QString &id, const QList<int> &context)
{
    OverrideableAction *a = 0;
    ICommand *c = registerOverridableAction(action, id, false);
    a = static_cast<OverrideableAction *>(c);
    if (a)
        a->addOverrideAction(action, context);
    return a;
}

ICommand *ActionManagerPrivate::registerOverridableAction(QAction *action, const QString &id, bool checkUnique)
{
    OverrideableAction *a = 0;
    const int uid = m_mainWnd->uniqueIDManager()->uniqueIdentifier(id);
    if (Command *c = m_idCmdMap.value(uid, 0)) {
        if (c->type() != ICommand::CT_OverridableAction) {
            qWarning() << "registerAction: id" << id << "is registered with a different command type.";
            return c;
        }
        a = static_cast<OverrideableAction *>(c);
    }
    if (!a) {
        a = new OverrideableAction(uid);
        m_idCmdMap.insert(uid, a);
    }

    if (!a->action()) {
        QAction *baseAction = new QAction(m_mainWnd);
        baseAction->setObjectName(id);
        baseAction->setCheckable(action->isCheckable());
        baseAction->setIcon(action->icon());
        baseAction->setIconText(action->iconText());
        baseAction->setText(action->text());
        baseAction->setToolTip(action->toolTip());
        baseAction->setStatusTip(action->statusTip());
        baseAction->setWhatsThis(action->whatsThis());
        baseAction->setChecked(action->isChecked());
        baseAction->setSeparator(action->isSeparator());
        baseAction->setShortcutContext(Qt::ApplicationShortcut);
        baseAction->setEnabled(false);
        baseAction->setObjectName(id);
        baseAction->setParent(m_mainWnd);
#ifdef Q_OS_MAC
        baseAction->setIconVisibleInMenu(false);
#endif
        a->setAction(baseAction);
        m_mainWnd->addAction(baseAction);
        a->setKeySequence(a->keySequence());
        a->setDefaultKeySequence(QKeySequence());
    } else  if (checkUnique) {
        qWarning() << "registerOverridableAction: id" << id << "is already registered.";
    }

    return a;
}

ICommand *ActionManagerPrivate::registerShortcut(QShortcut *shortcut, const QString &id, const QList<int> &context)
{
    Shortcut *sc = 0;
    int uid = m_mainWnd->uniqueIDManager()->uniqueIdentifier(id);
    if (Command *c = m_idCmdMap.value(uid, 0)) {
        if (c->type() != ICommand::CT_Shortcut) {
            qWarning() << "registerShortcut: id" << id << "is registered with a different command type.";
            return c;
        }
        sc = static_cast<Shortcut *>(c);
    } else {
        sc = new Shortcut(uid);
        m_idCmdMap.insert(uid, sc);
    }

    if (sc->shortcut()) {
        qWarning() << "registerShortcut: action already registered (id" << id << ".";
        return sc;
    }

    if (!hasContext(context))
        shortcut->setEnabled(false);
    shortcut->setObjectName(id);
    shortcut->setParent(m_mainWnd);
    sc->setShortcut(shortcut);

    if (context.isEmpty())
        sc->setContext(QList<int>() << 0);
    else
        sc->setContext(context);

    sc->setKeySequence(shortcut->key());
    sc->setDefaultKeySequence(QKeySequence());

    return sc;
}

ICommand *ActionManagerPrivate::command(const QString &id) const
{
    const int uid = m_mainWnd->uniqueIDManager()->uniqueIdentifier(id);
    const IdCmdMap::const_iterator it = m_idCmdMap.constFind(uid);
    if (it == m_idCmdMap.constEnd()) {
        if (warnAboutFindFailures)
            qWarning() << "ActionManagerPrivate::command(): failed to find :" << id << '/' << uid;
        return 0;
    }
    return it.value();
}

IActionContainer *ActionManagerPrivate::actionContainer(const QString &id) const
{
    const int uid = m_mainWnd->uniqueIDManager()->uniqueIdentifier(id);
    const IdContainerMap::const_iterator it =  m_idContainerMap.constFind(uid);
    if ( it == m_idContainerMap.constEnd()) {
        if (warnAboutFindFailures)
            qWarning() << "ActionManagerPrivate::actionContainer(): failed to find :" << id << '/' << uid;
        return 0;
    }
    return it.value();
}

ICommand *ActionManagerPrivate::command(int uid) const
{
    const IdCmdMap::const_iterator it = m_idCmdMap.constFind(uid);
    if (it == m_idCmdMap.constEnd()) {
        if (warnAboutFindFailures)
            qWarning() << "ActionManagerPrivate::command(): failed to find :" <<  m_mainWnd->uniqueIDManager()->stringForUniqueIdentifier(uid) << '/' << uid;
        return 0;
    }
    return it.value();
}

IActionContainer *ActionManagerPrivate::actionContainer(int uid) const
{
    const IdContainerMap::const_iterator it = m_idContainerMap.constFind(uid);
    if (it == m_idContainerMap.constEnd()) {
        if (warnAboutFindFailures)
            qWarning() << "ActionManagerPrivate::actionContainer(): failed to find :" <<  m_mainWnd->uniqueIDManager()->stringForUniqueIdentifier(uid) << uid;
        return 0;
    }
    return it.value();
}

static const char *settingsGroup = "KeyBindings";
static const char *idKey = "ID";
static const char *sequenceKey = "Keysequence";

void ActionManagerPrivate::initialize()
{
    QSettings *settings = m_mainWnd->settings();
    const int shortcuts = settings->beginReadArray(QLatin1String(settingsGroup));
    for (int i=0; i<shortcuts; ++i) {
        settings->setArrayIndex(i);
        const QString sid = settings->value(QLatin1String(idKey)).toString();
        const QKeySequence key(settings->value(QLatin1String(sequenceKey)).toString());
        const int id = m_mainWnd->uniqueIDManager()->uniqueIdentifier(sid);

        ICommand *cmd = command(id);
        if (cmd)
            cmd->setKeySequence(key);
    }
    settings->endArray();
}

void ActionManagerPrivate::saveSettings(QSettings *settings)
{
    settings->beginWriteArray(QLatin1String(settingsGroup));
    int count = 0;

    const IdCmdMap::const_iterator cmdcend = m_idCmdMap.constEnd();
    for (IdCmdMap::const_iterator j = m_idCmdMap.constBegin(); j != cmdcend; ++j) {
        const int id = j.key();
        Command *cmd = j.value();
        QKeySequence key = cmd->keySequence();
        if (key != cmd->defaultKeySequence()) {
            const QString sid = m_mainWnd->uniqueIDManager()->stringForUniqueIdentifier(id);
            settings->setArrayIndex(count);
            settings->setValue(QLatin1String(idKey), sid);
            settings->setValue(QLatin1String(sequenceKey), key.toString());
            count++;
        }
    }

    settings->endArray();
}
