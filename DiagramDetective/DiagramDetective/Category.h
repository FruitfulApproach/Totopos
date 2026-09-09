#pragma once

#include <QStringList>
#include <QList>
#include "Object.h"

class CategoryProp;
class Arrow;
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

	// An arrow of this category, between two of its objects. What kind of
	// arrow that is, is the category's business: BigCat makes functors.
	virtual Arrow* createArrow(const QString& name, Node* from, Node* to);
	// the same, named for you: f, g, h, ... (F, G, H in BigCat)
	Arrow* createCanvasArrow(Node* from, Node* to);
	QString nextArrowName() const { return letterName(m_nextArrowIndex, firstArrowLetter()); }

	// Is that label already taken anywhere in the diagram? A name means one
	// thing here, so a fresh one steps over what is already spoken for -
	// otherwise a category C would call its first object C too.
	bool nameInUse(const QString& name) const;

	// the name the next createCanvasObject will use
	QString nextObjectName() const { return letterName(m_nextIndex, firstLetter()); }
	// how far the naming has got, so a diagram read from a file carries on
	// where it left off instead of making a second C
	int nextObjectIndex() const { return m_nextIndex; }
	void setNextObjectIndex(int index) { m_nextIndex = index; }
	int nextArrowIndex() const { return m_nextArrowIndex; }
	void setNextArrowIndex(int index) { m_nextArrowIndex = index; }
	static QString letterName(int index, QChar first = QChar('C'));

	// Exactness of the diagram DRAWN IN HERE, asserted the way commuting is:
	// at every object along a row, the image of the arrow coming in is the
	// kernel of the arrow going out. Rows and columns are asserted separately
	// - a diagram may have exact rows and say nothing about its columns.
	bool rowsExact() const { return m_rowsExact; }
	bool columnsExact() const { return m_columnsExact; }
	void setRowsExact(bool exact);
	void setColumnsExact(bool exact);
	// the same, and put it in the scene's history
	void setRowsExactRecorded(bool exact);
	void setColumnsExactRecorded(bool exact);

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
	QRectF boundingRect() const override { return childFrame().adjusted(-9, -9, 9, 9); }

protected:
	// what this category lets you build, grouped under Construct
	void populateActions(QMenu& menu) override;

	// the next letter nothing has taken yet, advancing the counter past it
	QString freshName(int& counter, QChar first) const;

	// the kind of object this category is made of; the default is a plain object
	virtual Object* makeObject(const QString& name);
	// where the object names start
	virtual QChar firstLetter() const { return QChar('C'); }
	// where the arrow names start (lower case stays lower case)
	virtual QChar firstArrowLetter() const { return QChar('f'); }

public:
	// What an arrow of this category is called: a functor in BigCat, an
	// R-linear map in Mod-R, a homomorphism in Grp. Used where the program
	// talks about one.
	virtual QString morphismName() const { return QStringLiteral("arrow"); }
	// and what an OBJECT of it is called: an R-module, a set, a category
	virtual QString objectName() const { return QStringLiteral("object"); }

	// What an arrow drawn WITHOUT a label means here. In an additive category
	// there is one arrow that needs no name - the zero map - so a blank label
	// is read as 0 wherever the name is needed, while the label itself stays
	// blank on the canvas. Anywhere else a blank label is simply an arrow
	// with no name.
	virtual QString implicitArrowName() const { return QString(); }

protected:

	// nested categories fade, so the yellow does not pile up level on level
	void applyDepthAppearance(int depth) override;

private:
	bool m_rowsExact = false;
	bool m_columnsExact = false;
	int m_nextIndex = 0;
	int m_nextArrowIndex = 0;
	QList<CategoryProp*> m_props;
};
