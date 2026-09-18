/****************************************************************************
** Meta object code from reading C++ file 'Arrow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../art/Arrow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Arrow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN5ArrowE_t {};
} // unnamed namespace

template <> constexpr inline auto Arrow::qt_create_metaobjectdata<qt_meta_tag_ZN5ArrowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Arrow",
        "styleChanged",
        "",
        "Arrow*",
        "arrow",
        "domainChanged",
        "Node*",
        "domain",
        "codomainChanged",
        "codomain",
        "bendsChanged",
        "onObjectDeleted",
        "object",
        "onObjectMoved",
        "QPointF"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'styleChanged'
        QtMocHelpers::SignalData<void(Arrow *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'domainChanged'
        QtMocHelpers::SignalData<void(Node *)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'codomainChanged'
        QtMocHelpers::SignalData<void(Node *)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 9 },
        }}),
        // Signal 'bendsChanged'
        QtMocHelpers::SignalData<void(Arrow *)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'onObjectDeleted'
        QtMocHelpers::SlotData<void(Node *)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 6, 12 },
        }}),
        // Slot 'onObjectMoved'
        QtMocHelpers::SlotData<void(Node *, const QPointF &)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 6, 2 }, { 0x80000000 | 14, 2 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Arrow, qt_meta_tag_ZN5ArrowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Arrow::staticMetaObject = { {
    QMetaObject::SuperData::link<Node::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN5ArrowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN5ArrowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN5ArrowE_t>.metaTypes,
    nullptr
} };

void Arrow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Arrow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->styleChanged((*reinterpret_cast<std::add_pointer_t<Arrow*>>(_a[1]))); break;
        case 1: _t->domainChanged((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1]))); break;
        case 2: _t->codomainChanged((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1]))); break;
        case 3: _t->bendsChanged((*reinterpret_cast<std::add_pointer_t<Arrow*>>(_a[1]))); break;
        case 4: _t->onObjectDeleted((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1]))); break;
        case 5: _t->onObjectMoved((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
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
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Arrow* >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Node* >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Node* >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Arrow* >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Node* >(); break;
            }
            break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Node* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Arrow::*)(Arrow * )>(_a, &Arrow::styleChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Arrow::*)(Node * )>(_a, &Arrow::domainChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Arrow::*)(Node * )>(_a, &Arrow::codomainChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Arrow::*)(Arrow * )>(_a, &Arrow::bendsChanged, 3))
            return;
    }
}

const QMetaObject *Arrow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Arrow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN5ArrowE_t>.strings))
        return static_cast<void*>(this);
    return Node::qt_metacast(_clname);
}

int Arrow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Node::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void Arrow::styleChanged(Arrow * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void Arrow::domainChanged(Node * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void Arrow::codomainChanged(Node * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void Arrow::bendsChanged(Arrow * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
