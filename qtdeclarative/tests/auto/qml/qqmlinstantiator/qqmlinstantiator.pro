CONFIG += testcase
TARGET = tst_qqmlinstantiator
macx:CONFIG -= app_bundle

INCLUDEPATH += ../../shared/
SOURCES += tst_qqmlinstantiator.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
