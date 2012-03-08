/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <qtest.h>
#include <QDebug>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlproperty.h>
#include <QtQml/qqmlincubator.h>
#include <qcolor.h>
#include "../../shared/util.h"

class MyIC : public QObject, public QQmlIncubationController
{
    Q_OBJECT
public:
    MyIC() { startTimer(5); }
protected:
    virtual void timerEvent(QTimerEvent*) {
        incubateFor(5);
    }
};

class tst_qqmlcomponent : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlcomponent() { engine.setIncubationController(&ic); }

private slots:
    void null();
    void loadEmptyUrl();
    void qmlCreateObject();
    void qmlCreateObjectWithProperties();
    void qmlIncubateObject();
    void qmlCreateParentReference();

private:
    QQmlEngine engine;
    MyIC ic;
};

void tst_qqmlcomponent::null()
{
    {
        QQmlComponent c;
        QVERIFY(c.isNull());
    }

    {
        QQmlComponent c(&engine);
        QVERIFY(c.isNull());
    }
}


void tst_qqmlcomponent::loadEmptyUrl()
{
    QQmlComponent c(&engine);
    c.loadUrl(QUrl());

    QVERIFY(c.isError());
    QCOMPARE(c.errors().count(), 1);
    QQmlError error = c.errors().first();
    QCOMPARE(error.url(), QUrl());
    QCOMPARE(error.line(), -1);
    QCOMPARE(error.column(), -1);
    QCOMPARE(error.description(), QLatin1String("Invalid empty URL"));
}

void tst_qqmlcomponent::qmlIncubateObject()
{
    QQmlComponent component(&engine, testFileUrl("incubateObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), false);

    QTRY_VERIFY(object->property("test2").toBool() == true);

    delete object;
}

void tst_qqmlcomponent::qmlCreateObject()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QObject *testObject1 = object->property("qobject").value<QObject*>();
    QVERIFY(testObject1);
    QVERIFY(testObject1->parent() == object);

    QObject *testObject2 = object->property("declarativeitem").value<QObject*>();
    QVERIFY(testObject2);
    QVERIFY(testObject2->parent() == object);
    QCOMPARE(testObject2->metaObject()->className(), "QQuickItem");
}

void tst_qqmlcomponent::qmlCreateObjectWithProperties()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createObjectWithScript.qml"));
    QVERIFY2(component.errorString().isEmpty(), component.errorString().toUtf8());
    QObject *object = component.create();
    QVERIFY(object != 0);

    QObject *testObject1 = object->property("declarativerectangle").value<QObject*>();
    QVERIFY(testObject1);
    QVERIFY(testObject1->parent() == object);
    QCOMPARE(testObject1->property("x").value<int>(), 17);
    QCOMPARE(testObject1->property("y").value<int>(), 17);
    QCOMPARE(testObject1->property("color").value<QColor>(), QColor(255,255,255));
    QCOMPARE(QQmlProperty::read(testObject1,"border.width").toInt(), 3);
    QCOMPARE(QQmlProperty::read(testObject1,"innerRect.border.width").toInt(), 20);
    delete testObject1;

    QObject *testObject2 = object->property("declarativeitem").value<QObject*>();
    QVERIFY(testObject2);
    QVERIFY(testObject2->parent() == object);
    //QCOMPARE(testObject2->metaObject()->className(), "QDeclarativeItem_QML_2");
    QCOMPARE(testObject2->property("x").value<int>(), 17);
    QCOMPARE(testObject2->property("y").value<int>(), 17);
    QCOMPARE(testObject2->property("testBool").value<bool>(), true);
    QCOMPARE(testObject2->property("testInt").value<int>(), 17);
    QCOMPARE(testObject2->property("testObject").value<QObject*>(), object);
    delete testObject2;

    QObject *testBindingObj = object->property("bindingTestObject").value<QObject*>();
    QVERIFY(testBindingObj);
    QCOMPARE(testBindingObj->parent(), object);
    QCOMPARE(testBindingObj->property("testValue").value<int>(), 300);
    object->setProperty("width", 150);
    QCOMPARE(testBindingObj->property("testValue").value<int>(), 150 * 3);
    delete testBindingObj;

    QObject *testBindingThisObj = object->property("bindingThisTestObject").value<QObject*>();
    QVERIFY(testBindingThisObj);
    QCOMPARE(testBindingThisObj->parent(), object);
    QCOMPARE(testBindingThisObj->property("testValue").value<int>(), 900);
    testBindingThisObj->setProperty("width", 200);
    QCOMPARE(testBindingThisObj->property("testValue").value<int>(), 200 * 3);
    delete testBindingThisObj;
}

static QStringList warnings;
static void msgHandler(QtMsgType, const char *warning)
{
    warnings << QString::fromUtf8(warning);
}

void tst_qqmlcomponent::qmlCreateParentReference()
{
    QQmlEngine engine;

    QCOMPARE(engine.outputWarningsToStandardError(), true);

    warnings.clear();
    QtMsgHandler old = qInstallMsgHandler(msgHandler);

    QQmlComponent component(&engine, testFileUrl("createParentReference.qml"));
    QVERIFY2(component.errorString().isEmpty(), component.errorString().toUtf8());
    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(QMetaObject::invokeMethod(object, "createChild"));
    delete object;

    qInstallMsgHandler(old);

    engine.setOutputWarningsToStandardError(false);
    QCOMPARE(engine.outputWarningsToStandardError(), false);

    QCOMPARE(warnings.count(), 0);
}

QTEST_MAIN(tst_qqmlcomponent)

#include "tst_qqmlcomponent.moc"