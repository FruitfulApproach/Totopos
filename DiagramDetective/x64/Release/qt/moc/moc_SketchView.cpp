/****************************************************************************
** Meta object code from reading C++ file 'SketchView.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../widget/SketchView.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SketchView.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10SketchViewE_t {};
} // unnamed namespace

template <> constexpr inline auto SketchView::qt_create_metaobjectdata<qt_meta_tag_ZN10SketchViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SketchView",
        "categoryChanged",
        "",
        "name",
        "categoryDefined",
        "properties",
        "commutesChanged",
        "commutes",
        "chaseRequested",
        "statementKindPicked",
        "kind",
        "statementNamed",
        "dropTargetChanged",
        "category",
        "canvasActionTriggered",
        "id",
        "toggleMenu",
        "setMenuOpen",
        "open",
        "fitTo",
        "QRectF",
        "sceneRect",
        "setStatement",
        "statement",
        "centreOnContents",
        "fitContents",
        "setChasing",
        "chasing",
        "setCommutes",
        "setStatementKind",
        "carryFragment",
        "payload",
        "sceneCentre",
        "QPointF"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'categoryChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'categoryDefined'
        QtMocHelpers::SignalData<void(const QString &, const QStringList &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QStringList, 5 },
        }}),
        // Signal 'commutesChanged'
        QtMocHelpers::SignalData<void(bool)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 7 },
        }}),
        // Signal 'chaseRequested'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statementKindPicked'
        QtMocHelpers::SignalData<void(int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Signal 'statementNamed'
        QtMocHelpers::SignalData<void(const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'dropTargetChanged'
        QtMocHelpers::SignalData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Signal 'canvasActionTriggered'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'toggleMenu'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setMenuOpen'
        QtMocHelpers::SlotData<void(bool)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'fitTo'
        QtMocHelpers::SlotData<void(const QRectF &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 20, 21 },
        }}),
        // Slot 'setStatement'
        QtMocHelpers::SlotData<void(const QString &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Slot 'centreOnContents'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'fitContents'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setChasing'
        QtMocHelpers::SlotData<void(bool)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 27 },
        }}),
        // Slot 'setCommutes'
        QtMocHelpers::SlotData<void(bool)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 7 },
        }}),
        // Slot 'setStatementKind'
        QtMocHelpers::SlotData<void(int, const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 }, { QMetaType::QString, 3 },
        }}),
        // Slot 'carryFragment'
        QtMocHelpers::SlotData<void(const QByteArray &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 31 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'sceneCentre'
        QtMocHelpers::PropertyData<QPointF>(32, 0x80000000 | 33, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SketchView, qt_meta_tag_ZN10SketchViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SketchView::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsView::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10SketchViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10SketchViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10SketchViewE_t>.metaTypes,
    nullptr
} };

void SketchView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SketchView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->categoryChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->categoryDefined((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[2]))); break;
        case 2: _t->commutesChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->chaseRequested(); break;
        case 4: _t->statementKindPicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->statementNamed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->dropTargetChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->canvasActionTriggered((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->toggleMenu(); break;
        case 9: _t->setMenuOpen((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->fitTo((*reinterpret_cast<std::add_pointer_t<QRectF>>(_a[1]))); break;
        case 11: _t->setStatement((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->centreOnContents(); break;
        case 13: _t->fitContents(); break;
        case 14: _t->setChasing((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->setCommutes((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->setStatementKind((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 17: _t->carryFragment((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(const QString & )>(_a, &SketchView::categoryChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(const QString & , const QStringList & )>(_a, &SketchView::categoryDefined, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(bool )>(_a, &SketchView::commutesChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)()>(_a, &SketchView::chaseRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(int )>(_a, &SketchView::statementKindPicked, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(const QString & )>(_a, &SketchView::statementNamed, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(const QString & )>(_a, &SketchView::dropTargetChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (SketchView::*)(const QString & )>(_a, &SketchView::canvasActionTriggered, 7))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QPointF*>(_v) = _t->sceneCentre(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setSceneCentre(*reinterpret_cast<QPointF*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *SketchView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SketchView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10SketchViewE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsView::qt_metacast(_clname);
}

int SketchView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsView::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 18;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void SketchView::categoryChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SketchView::categoryDefined(const QString & _t1, const QStringList & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void SketchView::commutesChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SketchView::chaseRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void SketchView::statementKindPicked(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void SketchView::statementNamed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void SketchView::dropTargetChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void SketchView::canvasActionTriggered(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}
QT_WARNING_POP
