#pragma once

#include <QStringList>
#include <functional>
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

	// The actions that BUILD something. They go under Construct, in a named
	// group: `group("Limits")` finds or makes that submenu, and group("")
	// is Construct itself. A property that constructs nothing does nothing.
	using MenuGroup = std::function<QMenu*(const QString&)>;
	virtual void addConstructions(Category* category, const MenuGroup& group)
	{ Q_UNUSED(category); Q_UNUSED(group); }

	// the registry of built-in category properties (CategoryProps.cpp)
	static QStringList keys();
	// a fresh property by key, owned by `category`; nullptr for an unknown key
	static CategoryProp* create(const QString& key, Category* category);

private:
	Category* m_category = nullptr;
};
