#pragma once

#include "CategoryProp.h"
#include "HasProducts.h"

// The built-in properties a category may have. Each is its own class so that
// rules can ask a category `has<HasProducts>()`-style questions later on.

class HasCoproducts : public CategoryProp
{
	Q_OBJECT
public:
	explicit HasCoproducts(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("hasCoproducts"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has coproducts"); }
	QString description() const override { return QStringLiteral("Every pair of objects has a coproduct A + B with its injections."); }
};

class HasEqualizers : public CategoryProp
{
	Q_OBJECT
public:
	explicit HasEqualizers(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("hasEqualizers"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has equalizers"); }
	QString description() const override { return QStringLiteral("Every parallel pair f, g : A -> B has an equalizer."); }
};

class HasCoequalizers : public CategoryProp
{
	Q_OBJECT
public:
	explicit HasCoequalizers(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("hasCoequalizers"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has coequalizers"); }
	QString description() const override { return QStringLiteral("Every parallel pair f, g : A -> B has a coequalizer."); }
};

class HasZeroObject : public CategoryProp
{
	Q_OBJECT
public:
	explicit HasZeroObject(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("hasZeroObject"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has a zero object"); }
	QString description() const override { return QStringLiteral("An object 0 that is both initial and terminal."); }
};

class HasKernels : public CategoryProp
{
	Q_OBJECT
public:
	explicit HasKernels(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("hasKernels"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has kernels and cokernels"); }
	QString description() const override { return QStringLiteral("Every morphism has a kernel and a cokernel."); }
};

class IsAdditive : public CategoryProp
{
	Q_OBJECT
public:
	explicit IsAdditive(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("isAdditive"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Additive (biproducts, hom-groups)"); }
	QString description() const override { return QStringLiteral("Hom-sets are abelian groups, composition is bilinear, finite biproducts exist."); }
};

class IsAbelian : public CategoryProp
{
	Q_OBJECT
public:
	explicit IsAbelian(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("isAbelian"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Abelian"); }
	QString description() const override { return QStringLiteral("Additive, with kernels and cokernels, every mono a kernel and every epi a cokernel."); }
};

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
