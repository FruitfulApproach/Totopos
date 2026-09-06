#pragma once

#include <QStringList>
#include <QList>
#include "Object.h"

class CategoryProp;
class QMenu;

// A category drawn as an object; the objects placed "in" it are its child
// items. What a double-click places, and how it is named, is the category's
// business: the built-ins (categories/) override makeObject and firstLetter;
// a custom category places plain objects.
class Category  : public Object
{
	Q_OBJECT

public:
	Category(const QString& name, QGraphicsItem* parent = nullptr);
	~Category();

	// the registry of built-ins (categories/BuiltInCategories.cpp)
	static QStringList builtInNames();
	// a fresh built-in by name, or nullptr when the name is not built in
	static Category* createBuiltIn(const QString& name, QGraphicsItem* parent = nullptr);

	// Put a fresh object of this category at a scene position, as a child of
	// this category: makeObject decides the kind, firstLetter the naming
	// (C, D, ..., Z, then A', B', ..., then A'', ...).
	Object* createCanvasObject(const QPointF& scenePos);

	// Take an existing node in as a child, keeping it at the given scene
	// position (where the user sees it).
	void adopt(Node* node, const QPointF& scenePos);

	// A named object of the kind this category is made of (a category in
	// BigCat, a set in Set, ...), as a child at a scene position. Properties
	// that build objects (products, ...) go through here, never `new Object`.
	Object* createObject(const QString& name, const QPointF& scenePos);

	// the name the next createCanvasObject will use
	QString nextObjectName() const { return letterName(m_nextIndex, firstLetter()); }
	static QString letterName(int index, QChar first = QChar('C'));

	// The structure the category is known to have, as CategoryProp objects it
	// owns (props/CategoryProps.h: HasProducts, IsAbelian, ...).
	const QList<CategoryProp*>& props() const { return m_props; }
	// the same by key: "hasProducts", "isAbelian", ... (the dialog's checkbox names)
	QStringList properties() const;
	void setProperties(const QStringList& keys);
	void addProperty(const QString& key);
	bool has(const QString& key) const;
	template <typename P> bool has() const { return has(P::Key()); }

	// a category frames its objects with more room than a plain object gives its label
	QRectF boundingRect() const override { return childrenBoundingRect().adjusted(-18, -18, 18, 18); }

protected:
	// the node's own menu, then a section from every property that has one
	void populateContextMenu(QMenu& menu) override;

	// the kind of object this category is made of; the default is a plain object
	virtual Object* makeObject(const QString& name);
	// where the object names start
	virtual QChar firstLetter() const { return QChar('C'); }

private:
	int m_nextIndex = 0;
	QList<CategoryProp*> m_props;
};
