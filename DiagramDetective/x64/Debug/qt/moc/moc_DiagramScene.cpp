/****************************************************************************
** Meta object code from reading C++ file 'DiagramScene.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../art/DiagramScene.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DiagramScene.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN12DiagramSceneE_t {};
} // unnamed namespace

template <> constexpr inline auto DiagramScene::qt_create_metaobjectdata<qt_meta_tag_ZN12DiagramSceneE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DiagramScene",
        "ambientCategoryChanged",
        "",
        "Category*",
        "category",
        "message",
        "text",
        "chasingChanged",
        "chasing",
        "commutesChanged",
        "commutes",
        "statementKindChanged",
        "kind",
        "name",
        "error",
        "nodesAdded",
        "QList<Node*>",
        "nodes",
        "ruleChanged",
        "matches",
        "nodesRemoved",
        "statementChanged",
        "statement",
        "fragmentDragRequested",
        "payload",
        "setAmbientCategory"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'ambientCategoryChanged'
        QtMocHelpers::SignalData<void(Category *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'message'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'chasingChanged'
        QtMocHelpers::SignalData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Signal 'commutesChanged'
        QtMocHelpers::SignalData<void(bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 },
        }}),
        // Signal 'statementKindChanged'
        QtMocHelpers::SignalData<void(int, const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::QString, 13 },
        }}),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'nodesAdded'
        QtMocHelpers::SignalData<void(const QList<Node*> &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Signal 'ruleChanged'
        QtMocHelpers::SignalData<void(const QString &, int)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 }, { QMetaType::Int, 19 },
        }}),
        // Signal 'nodesRemoved'
        QtMocHelpers::SignalData<void(const QList<Node*> &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Signal 'statementChanged'
        QtMocHelpers::SignalData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
        // Signal 'fragmentDragRequested'
        QtMocHelpers::SignalData<void(const QByteArray &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 24 },
        }}),
        // Slot 'setAmbientCategory'
        QtMocHelpers::SlotData<void(const QString &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DiagramScene, qt_meta_tag_ZN12DiagramSceneE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DiagramScene::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsScene::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12DiagramSceneE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12DiagramSceneE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12DiagramSceneE_t>.metaTypes,
    nullptr
} };

void DiagramScene::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DiagramScene *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->ambientCategoryChanged((*reinterpret_cast<std::add_pointer_t<Category*>>(_a[1]))); break;
        case 1: _t->message((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->chasingChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->commutesChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->statementKindChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->error((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->nodesAdded((*reinterpret_cast<std::add_pointer_t<QList<Node*>>>(_a[1]))); break;
        case 7: _t->ruleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->nodesRemoved((*reinterpret_cast<std::add_pointer_t<QList<Node*>>>(_a[1]))); break;
        case 9: _t->statementChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->fragmentDragRequested((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 11: _t->setAmbientCategory((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Category* >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Node*> >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Node*> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(Category * )>(_a, &DiagramScene::ambientCategoryChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QString & )>(_a, &DiagramScene::message, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(bool )>(_a, &DiagramScene::chasingChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(bool )>(_a, &DiagramScene::commutesChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(int , const QString & )>(_a, &DiagramScene::statementKindChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QString & )>(_a, &DiagramScene::error, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QList<Node*> & )>(_a, &DiagramScene::nodesAdded, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QString & , int )>(_a, &DiagramScene::ruleChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QList<Node*> & )>(_a, &DiagramScene::nodesRemoved, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QString & )>(_a, &DiagramScene::statementChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagramScene::*)(const QByteArray & )>(_a, &DiagramScene::fragmentDragRequested, 10))
            return;
    }
}

const QMetaObject *DiagramScene::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DiagramScene::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12DiagramSceneE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsScene::qt_metacast(_clname);
}

int DiagramScene::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsScene::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void DiagramScene::ambientCategoryChanged(Category * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void DiagramScene::message(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void DiagramScene::chasingChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void DiagramScene::commutesChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void DiagramScene::statementKindChanged(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void DiagramScene::error(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void DiagramScene::nodesAdded(const QList<Node*> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void DiagramScene::ruleChanged(const QString & _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 8
void DiagramScene::nodesRemoved(const QList<Node*> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void DiagramScene::statementChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void DiagramScene::fragmentDragRequested(const QByteArray & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}
QT_WARNING_POP
