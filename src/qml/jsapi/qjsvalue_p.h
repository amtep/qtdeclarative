/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef QJSVALUE_P_H
#define QJSVALUE_P_H

#include <qjsvalue.h>
#include <private/qtqmlglobal_p.h>
#include <private/qv4value_p.h>
#include <private/qv4string_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4object_p.h>

QT_BEGIN_NAMESPACE

/*!
  \internal
  \class QJSValuePrivate
*/
class Q_QML_PRIVATE_EXPORT QJSValuePrivate : public QV4::PersistentValuePrivate
{
public:
    QJSValuePrivate(QV4::ExecutionEngine *engine, const QV4::Value &v)
        : PersistentValuePrivate(v, engine)
    {
        if (value.isEmpty())
            value = QV4::Value::undefinedValue();
    }
    QJSValuePrivate(QV4::Object *o)
        : PersistentValuePrivate(QV4::Value::fromObject(o))
    { }
    QJSValuePrivate(QV4::String *s)
        : PersistentValuePrivate(QV4::Value::fromString(s))
    { }
    QJSValuePrivate(const QString &s)
        : PersistentValuePrivate(QV4::Value::undefinedValue())
        , string(0, s)
    {
        value = QV4::Value::fromString(&string);
    }

    QV4::Value getValue(QV4::ExecutionEngine *e);

    static QJSValuePrivate *get(const QJSValue &v) { return v.d; }

    QV4::String string;
};

QT_END_NAMESPACE

#endif
