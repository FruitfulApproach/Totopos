/****************************************************************************
** Meta object code from reading C++ file 'MapsElements.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../core/props/MapsElements.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MapsElements.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12MapsElementsE_t {};
} // unnamed namespace

template <> constexpr inline auto MapsElements::qt_create_metaobjectdata<qt_meta_tag_ZN12MapsElementsE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MapsElements",
        "settingsChanged",
        "",
        "onFunctorRenamed",
        "onSourceBends",
        "Arrow*",
        "source",
        "onImageBends",
        "image",
        "onSourceMoved",
        "Node*",
        "QPointF",
        "delta",
        "onImageMoved",
        "onSourceDeleted"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'settingsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onFunctorRenamed'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSourceBends'
        QtMocHelpers::SlotData<void(Arrow *)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Slot 'onImageBends'
        QtMocHelpers::SlotData<void(Arrow *)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 5, 8 },
        }}),
        // Slot 'onSourceMoved'
        QtMocHelpers::SlotData<void(Node *, const QPointF &)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 6 }, { 0x80000000 | 11, 12 },
        }}),
        // Slot 'onImageMoved'
        QtMocHelpers::SlotData<void(Node *, const QPointF &)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 8 }, { 0x80000000 | 11, 12 },
        }}),
        // Slot 'onSourceDeleted'
        QtMocHelpers::SlotData<void(Node *)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 6 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MapsElements, qt_meta_tag_ZN12MapsElementsE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MapsElements::staticMetaObject = { {
    QMetaObject::SuperData::link<ArrowProp::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12MapsElementsE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12MapsElementsE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12MapsElementsE_t>.metaTypes,
    nullptr
} };

void MapsElements::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MapsElements *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->settingsChanged(); break;
        case 1: _t->onFunctorRenamed(); break;
        case 2: _t->onSourceBends((*reinterpret_cast<std::add_pointer_t<Arrow*>>(_a[1]))); break;
        case 3: _t->onImageBends((*reinterpret_cast<std::add_pointer_t<Arrow*>>(_a[1]))); break;
        case 4: _t->onSourceMoved((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 5: _t->onImageMoved((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 6: _t->onSourceDeleted((*reinterpret_cast<std::add_pointer_t<Node*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MapsElements::*)()>(_a, &MapsElements::settingsChanged, 0))
            return;
    }
}

const QMetaObject *MapsElements::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MapsElements::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12MapsElementsE_t>.strings))
        return static_cast<void*>(this);
    return ArrowProp::qt_metacast(_clname);
}

int MapsElements::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = ArrowProp::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void MapsElements::settingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
