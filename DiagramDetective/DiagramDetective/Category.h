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

	// Which built-in this category IS, by the name the registry knows it
	// under: "R-Mod", "BigCat", ... Empty for one the user defined, which is
	// a plain Category carrying whatever structure was ticked. This is not
	// id(): a category can be renamed and still be R-Mod.
	virtual QString builtInName() const { return QString(); }

	// A fresh category of the SAME KIND as another - the same built-in class,
	// or a plain one carrying the same structure. This is what a subcategory
	// is made with: a subcategory of R-Mod has to be an R-Mod, so that what
	// is placed in it is an R-module and what is drawn between them is an
	// R-linear map.
	static Category* createLike(const Category* model, const QString& name, QGraphicsItem* parent = nullptr);

	// A category drawn INSIDE another and asserted to be a subcategory of it:
	// its objects are objects of the surrounding category, and its arrows are
	// arrows of it. Drawn with a dotted border and a fainter fill, so it can
	// be told from an object of that category at a glance. Nesting has no
	// limit - a subcategory of a subcategory of R-Mod is still R-modules.
	bool isSubcategory() const { return m_subcategory; }
	void setSubcategory(bool subcategory);
	// the category this is a subcategory OF, or nullptr when it is not one
	Category* ambient() const;

	// Place a fresh subcategory of THIS category at a scene position, as a
	// child. It is made as a category of the same kind - a subcategory of
	// R-Mod is an R-Mod - so what is placed inside it is an R-module and what
	// is drawn between them an R-linear map, and it can hold subcategories of
	// its own with no limit to the nesting.
	Category* createSubcategory(const QPointF& scenePos);
	// the next letter free for a subcategory: S, T, U, ... then S', T', ...
	QString nextSubcategoryName() const;

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

	// Whether the diagram drawn IN HERE is asserted to commute. Off is not
	// the claim that it fails to commute: it is the absence of a claim. The
	// ambient category is the whole canvas, and keeps this on the scene
	// instead, so that the panel over the canvas and this page never disagree.
	bool commutes() const;
	void setCommutes(bool commutes);

	// What the diagram drawn in here is put forward AS, and what it is
	// called. The same routing as commuting: the ambient category answers
	// for the scene.
	int statementKind() const;
	void setStatementKind(int kind);
	QString statementName() const;
	void setStatementName(const QString& name);

	// is this the category everything else is drawn in?
	bool isAmbient() const;

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

	// "Category C", or "Subcategory S of R-Mod" when it is one
	QString contextTitle() const override;

public:
	// The kind of object this category is made of; the default is a plain
	// object. Public, not protected: a subcategory has to ask the category
	// it sits in what its objects are, and C++ would not let it reach a
	// protected member through a pointer to another category.
	virtual Object* makeObject(const QString& name);
	// where the object names start
	virtual QChar firstLetter() const { return QChar('C'); }
	// where the arrow names start (lower case stays lower case)
	virtual QChar firstArrowLetter() const { return QChar('f'); }

protected:
	// what this category lets you build, grouped under Construct
	void populateActions(QMenu& menu) override;

	// the next letter nothing has taken yet, advancing the counter past it
	QString freshName(int& counter, QChar first) const;

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

	// an empty subcategory still shows its dotted frame: being one is a
	// claim about the diagram, not a look that waits for something to frame
	bool alwaysFramed() const override { return m_subcategory; }

private:
	bool m_rowsExact = false;
	bool m_columnsExact = false;
	bool m_subcategory = false;
	bool m_commutes = false;
	int m_statementKind = 0;
	QString m_statementName;
	int m_nextIndex = 0;
	int m_nextArrowIndex = 0;
	QList<CategoryProp*> m_props;
};
