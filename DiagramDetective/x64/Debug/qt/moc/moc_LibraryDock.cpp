/****************************************************************************
** Meta object code from reading C++ file 'LibraryDock.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../widget/LibraryDock.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LibraryDock.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11LibraryDockE_t {};
} // unnamed namespace

template <> constexpr inline auto LibraryDock::qt_create_metaobjectdata<qt_meta_tag_ZN11LibraryDockE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "LibraryDock",
        "opened",
        "",
        "path",
        "removed",
        "renamed",
        "before",
        "after",
        "chosen",
        "applyAllRequested",
        "stopRequested",
        "rescan",
        "seed",
        "setRuleState",
        "name",
        "matches",
        "showContextMenu",
        "QPoint",
        "at",
        "renameFile",
        "removeFile"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'opened'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'removed'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'renamed'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 }, { QMetaType::QString, 7 },
        }}),
        // Signal 'chosen'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'applyAllRequested'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'stopRequested'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'rescan'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'seed'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setRuleState'
        QtMocHelpers::SlotData<void(const QString &, int)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 }, { QMetaType::Int, 15 },
        }}),
        // Slot 'showContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
        // Slot 'renameFile'
        QtMocHelpers::SlotData<void(const QString &)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'removeFile'
        QtMocHelpers::SlotData<void(const QString &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LibraryDock, qt_meta_tag_ZN11LibraryDockE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject LibraryDock::staticMetaObject = { {
    QMetaObject::SuperData::link<QDockWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11LibraryDockE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11LibraryDockE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11LibraryDockE_t>.metaTypes,
    nullptr
} };

void LibraryDock::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LibraryDock *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->opened((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->removed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->renamed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->chosen((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->applyAllRequested(); break;
        case 5: _t->stopRequested(); break;
        case 6: _t->rescan(); break;
        case 7: _t->seed(); break;
        case 8: _t->setRuleState((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->showContextMenu((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 10: _t->renameFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->removeFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LibraryDock::*)(const QString & )>(_a, &LibraryDock::opened, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LibraryDock::*)(const QString & )>(_a, &LibraryDock::removed, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LibraryDock::*)(const QString & , const QString & )>(_a, &LibraryDock::renamed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LibraryDock::*)(const QString & )>(_a, &LibraryDock::chosen, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (LibraryDock::*)()>(_a, &LibraryDock::applyAllRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (LibraryDock::*)()>(_a, &LibraryDock::stopRequested, 5))
            return;
    }
}

const QMetaObject *LibraryDock::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LibraryDock::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11LibraryDockE_t>.strings))
        return static_cast<void*>(this);
    return QDockWidget::qt_metacast(_clname);
}

int LibraryDock::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDockWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void LibraryDock::opened(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void LibraryDock::removed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void LibraryDock::renamed(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void LibraryDock::chosen(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void LibraryDock::applyAllRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void LibraryDock::stopRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
