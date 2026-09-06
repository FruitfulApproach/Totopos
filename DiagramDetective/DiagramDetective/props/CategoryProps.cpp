#include "CategoryProps.h"

QStringList CategoryProp::keys()
{
	return { HasProducts::Key(), HasCoproducts::Key(), HasEqualizers::Key(), HasCoequalizers::Key(), HasZeroObject::Key(), HasKernels::Key(), IsAdditive::Key(), IsAbelian::Key(), IsConcrete::Key(), IsLocallySmall::Key() };
}

CategoryProp* CategoryProp::create(const QString& key, Category* category)
{
	if (key == HasProducts::Key()) return new HasProducts(category);
	if (key == HasCoproducts::Key()) return new HasCoproducts(category);
	if (key == HasEqualizers::Key()) return new HasEqualizers(category);
	if (key == HasCoequalizers::Key()) return new HasCoequalizers(category);
	if (key == HasZeroObject::Key()) return new HasZeroObject(category);
	if (key == HasKernels::Key()) return new HasKernels(category);
	if (key == IsAdditive::Key()) return new IsAdditive(category);
	if (key == IsAbelian::Key()) return new IsAbelian(category);
	if (key == IsConcrete::Key()) return new IsConcrete(category);
	if (key == IsLocallySmall::Key()) return new IsLocallySmall(category);
	return nullptr;
}
