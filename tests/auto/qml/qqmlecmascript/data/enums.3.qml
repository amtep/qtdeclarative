import QtQuick 2.0
import Qt.test 1.0
import Qt.test 1.0 as Namespace

Item {
    // Enums from type
    property int a: Item.Center
    property int b: Item.Right

    // Enums from Qt
    property int c: Qt.blue
    property int d: Qt.darkRed

    // Enums from other type
    property int e: MyQmlObject.EnumValue3
    property int f: MyQmlObject.EnumValue4

    // Enums from namespaced other type
    property int h: Namespace.MyQmlObject.EnumValue3
    property int i: Namespace.MyQmlObject.EnumValue4

    // Count the onChanged signals to see whether
    // they're assigned as literals or via bindings
    property int ac: 0
    property int bc: 0
    property int cc: 0
    property int dc: 0
    property int ec: 0
    property int fc: 0
    property int hc: 0
    property int ic: 0

    onAChanged: ac++
    onBChanged: bc++
    onCChanged: cc++
    onDChanged: dc++
    onEChanged: ec++
    onFChanged: fc++
    onHChanged: hc++
    onIChanged: ic++
}