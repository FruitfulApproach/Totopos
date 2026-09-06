#pragma once

#include <QStringList>
#include "Prop.h"

class Category;
class QMenu;

// A proposition about a category: it has products, it is abelian, ...
class CategoryProp : public Prop
{
	Q_OBJECT

public:
	explicit CategoryProp(Category* category = nullptr);

	// the category this is about (its owner)
	Category* category() const { return m_category; }

	// What this property lets the user do from the category's right-click
	// menu (the canvas, for the ambient category): add a section with actions,
	// or nothing. Called by Category::populateContextMenu.
	virtual void categoryContextMenu(QMenu& menu, Category* category) { Q_UNUSED(menu); Q_UNUSED(category); }

	// the registry of built-in category properties (CategoryProps.cpp)
	static QStringList keys();
	// a fresh property by key, owned by `category`; nullptr for an unknown key
	static CategoryProp* create(const QString& key, Category* category);

private:
	Category* m_category = nullptr;
};
