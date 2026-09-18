#pragma once

#include "core/props/CategoryProp.h"
#include "core/props/Constructions.h"   // HasProducts, HasCoproducts, HasEqualizers, HasCoequalizers, HasZeroObject, HasKernels, IsAdditive, IsAbelian

// The remaining built-in properties are plain facts: nothing to construct.

class IsConcrete : public CategoryProp
{
	Q_OBJECT
public:
	explicit IsConcrete(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("isConcrete"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Concrete (a faithful functor to Set)"); }
	QString description() const override { return QStringLiteral("Objects have underlying sets and morphisms are functions between them."); }
};

class IsLocallySmall : public CategoryProp
{
	Q_OBJECT
public:
	explicit IsLocallySmall(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("isLocallySmall"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Locally small"); }
	QString description() const override { return QStringLiteral("Every hom-collection is a set."); }
};
