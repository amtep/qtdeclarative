/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickstatechangescript_p.h"

#include <qqml.h>
#include <qqmlcontext.h>
#include <qqmlexpression.h>
#include <qqmlinfo.h>
#include <private/qqmlguard_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlbinding_p.h>
#include "qquickstate_p_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qmath.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQuickStateChangeScriptPrivate : public QQuickStateOperationPrivate
{
public:
    QQuickStateChangeScriptPrivate() {}

    QQmlScriptString script;
    QString name;
};

/*!
    \qmlclass StateChangeScript QQuickStateChangeScript
    \inqmlmodule QtQuick 2
    \ingroup qml-state-elements
    \brief The StateChangeScript element allows you to run a script in a state.

    A StateChangeScript is run upon entering a state. You can optionally use
    ScriptAction to specify the point in the transition at which
    the StateChangeScript should to be run.

    \snippet snippets/declarative/states/statechangescript.qml state and transition

    \sa ScriptAction
*/

QQuickStateChangeScript::QQuickStateChangeScript(QObject *parent)
: QQuickStateOperation(*(new QQuickStateChangeScriptPrivate), parent)
{
}

QQuickStateChangeScript::~QQuickStateChangeScript()
{
}

/*!
    \qmlproperty script QtQuick2::StateChangeScript::script
    This property holds the script to run when the state is current.
*/
QQmlScriptString QQuickStateChangeScript::script() const
{
    Q_D(const QQuickStateChangeScript);
    return d->script;
}

void QQuickStateChangeScript::setScript(const QQmlScriptString &s)
{
    Q_D(QQuickStateChangeScript);
    d->script = s;
}

/*!
    \qmlproperty string QtQuick2::StateChangeScript::name
    This property holds the name of the script. This name can be used by a
    ScriptAction to target a specific script.

    \sa ScriptAction::scriptName
*/
QString QQuickStateChangeScript::name() const
{
    Q_D(const QQuickStateChangeScript);
    return d->name;
}

void QQuickStateChangeScript::setName(const QString &n)
{
    Q_D(QQuickStateChangeScript);
    d->name = n;
}

void QQuickStateChangeScript::execute(Reason)
{
    Q_D(QQuickStateChangeScript);
    if (!d->script.script().isEmpty()) {
        QQmlExpression expr(d->script);
        expr.evaluate();
        if (expr.hasError())
            qmlInfo(this, expr.error());
    }
}

QQuickStateChangeScript::ActionList QQuickStateChangeScript::actions()
{
    ActionList rv;
    QQuickAction a;
    a.event = this;
    rv << a;
    return rv;
}

QQuickActionEvent::EventType QQuickStateChangeScript::type() const
{
    return Script;
}


#include <moc_qquickstatechangescript_p.cpp>

QT_END_NAMESPACE
