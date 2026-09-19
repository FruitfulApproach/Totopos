#pragma once

#include <QStringList>
#include <QList>
#include "art/Object.h"

class CategoryProp;
class Arrow;
class QMenu;

// A category drawn as an object; the objects placed "in" it are its child
// items. What it places, and how it is named, is the category's business: the
// built-ins (categories/) override makeObject and firstLetter. A plain
// category makes PLAIN objects, which hold nothing in their turn - only
// BigCat and Cat, whose objects really are categories, make categories.
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

	// WHICH BUILT-IN DOES THIS LABEL MEAN? Empty when it means none of them.
	//
	// Somebody who types R-Mod on a category means the R-Mod that knows what
	// an R-module is - not a category that happens to be spelt that way and
	// knows nothing. So the name is read as the choice it plainly is, and the
	// node becomes that built-in.
	//
	// Said loosely on purpose: R-Mod, R-mod, RMod and "R Mod" are one name
	// written four ways, and nobody typing the third of them meant something
	// else by it. Case, spaces, hyphens and underscores are all ignored; the
	// letters in order are what is compared.
	static QString builtInNamed(const QString& label);

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
	QString nextArrowName() const { return freshName(m_nextArrowIndex, firstArrowLetter()); }

	// Is that label already taken anywhere in the diagram? A name means one
	// thing here, so a fresh one steps over what is already spoken for -
	// otherwise a category C would call its first object C too.
	bool nameInUse(const QString& name) const;

	// the name the next createCanvasObject will use
	QString nextObjectName() const { return freshName(m_nextIndex, firstLetter()); }
	// how far the naming has got, so a diagram read from a file carries on
	// where it left off instead of making a second C
	int nextObjectIndex() const { return m_nextIndex; }
	void setNextObjectIndex(int index) { m_nextIndex = index; }
	int nextArrowIndex() const { return m_nextArrowIndex; }
	void setNextArrowIndex(int index) { m_nextArrowIndex = index; }
	// index 0 is `first` itself; then round the alphabet `first` belongs to,
	// gaining a prime each time round. From X: X, Y, Z, A, ..., W, X', Y'.
	// Latin either case and Greek all work, so alpha' counts like B'.
	static QString letterName(int index, QChar first = QChar('X'));
	// The inverse, for a name that IS a plain variable - one letter and a run
	// of primes, in the same alphabet as `first`. -1 for anything else: a
	// word, a subscript, Hom(X,Y) - names a person chose rather than counted.
	static int variableIndex(const QString& text, QChar first);

	// A child of ours has just been given a name by hand. If it is a plain
	// variable the counting starts again from there, so renaming X to S makes
	// the next object T.
	void noteNamed(const Node* child, const QString& name);

	// Exactness of the diagram DRAWN IN HERE, asserted the way commuting is:
	// at every object along a row, the image of the arrow coming in is the
	// kernel of the arrow going out. Rows and columns are asserted separately
	// - a diagram may have exact rows and say nothing about its columns.
	// Is there such a thing as an exact sequence here?
	//
	// Exactness says that at each object along a row, the IMAGE of the arrow
	// coming in is the KERNEL of the arrow going out. That needs a zero
	// object, so there are zero morphisms to take a kernel of, and kernels to
	// take. In Set or Top the words mean nothing at all, and a switch
	// offering to assert them would be offering nonsense.
	bool exactnessDefined() const;

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

	// THE AMBIENT CATEGORY'S NAME IS NOT A NAME LIKE ANY OTHER.
	//
	// Everything drawn on the canvas is an object or an arrow OF it, so once
	// anything IS drawn the name has been committed to: retyping it here
	// would silently reinterpret every object and arrow already on the
	// canvas. That is exactly why the sketch panel's Category combo goes grey
	// with a padlock (SketchView::setCategoryLocked); the label in the scene
	// is the same choice by another face and locks with it.
	bool labelIsLocked() const override;
	QString labelLockTip() const override;

	// The structure the category is known to have, as CategoryProp objects it
	// owns (props/CategoryProps.h: HasProducts, IsAbelian, ...).
	const QList<CategoryProp*>& props() const { return m_props; }
	// the same by key: "hasProducts", "isAbelian", ... (the dialog's checkbox names)
	QStringList properties() const;
	void setProperties(const QStringList& keys);
	void addProperty(const QString& key);
	bool has(const QString& key) const;
	template <typename P> bool has() const { return has(P::Key()); }

	// ARE THIS CATEGORY'S OBJECTS SETS?
	//
	// A concrete category is one with a faithful functor to Set, which is to
	// say its objects HAVE underlying sets - and that is exactly what it takes
	// for "an element of X" to be a thing that can be named and drawn. R-Mod,
	// Ab, Grp, Set, Vect all say yes; BigCat and Cat say no, because an object
	// of theirs is a category, not a set.
	//
	// Virtual so a built-in that knows better can say so without carrying the
	// property; by default it IS the property.
	virtual bool objectsAreSets() const;

	// a category always holds things, and what it holds are OBJECTS
	bool canHoldNamedChildren() const override { return true; }
	Object* createNamedChild(const QString& name, const QPointF& scenePos) override
	{ return createObject(name, scenePos); }

	// a category frames its objects with more room than a plain object gives
	// its label - and an empty one is a ROOM rather than a name (see the note
	// on emptyFrame)
	QRectF boxRect() const override;

	// THE SIZE OF AN EMPTY CATEGORY: somewhere to put things, not a caption.
	//
	// A category with nothing in it used to be its own name with nine points
	// of air round it, which is the right answer for an object - an object IS
	// its name - and the wrong one for a category. A category is the place
	// its objects go, and a place you are meant to double-click into should
	// look like one before anything is in it: a sheet of it, plainly big
	// enough to hold a diagram, rather than a word you would not think to aim
	// at. It stops mattering the moment anything is drawn inside, because
	// from then on the frame is the union of what it holds.
	static QRectF emptyFrame();

	// THE SCENE HAS JUST MADE THIS ONE THE CANVAS.
	//
	// The ambient category is not a thing drawn on the diagram - it is what
	// the diagram is drawn IN - so it is not drawn as one: no fill, no
	// border, just its name in bold at the origin. Every other category is a
	// box you can see, because it is a thing standing somewhere.
	//
	// Called by the scene once isAmbient() would answer yes, which is the
	// earliest this can be put right: until then this category has no way of
	// knowing what it has become.
	void becameAmbient();

	// "Category C", or "Subcategory S of R-Mod" when it is one
	QString contextTitle() const override;

public:
	// The kind of object this category is made of; the default is a plain
	// object. Public, not protected: a subcategory has to ask the category
	// it sits in what its objects are, and C++ would not let it reach a
	// protected member through a pointer to another category.
	virtual Object* makeObject(const QString& name);
	// where the object names start
	// X, because an object of a category is a set-like thing and X, Y, Z is
	// what everyone writes. NOT related to the category's own name: a generic
	// category called C would otherwise start its objects at D, purely
	// because C had just been taken.
	virtual QChar firstLetter() const { return QChar('X'); }
	// where the arrow names start (lower case stays lower case)
	virtual QChar firstArrowLetter() const { return QChar('f'); }

protected:
	// what this category lets you build, grouped under Construct
	void populateActions(QMenu& menu) override;

	// the next letter nothing has taken yet, advancing the counter past it
	// the first unused name at or after `from` in the run starting at `first`
	QString freshName(int from, QChar first) const;

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

	// The canvas's name stays at the origin, in bold: there is no frame for
	// it to sit above, and nothing to settle it against.
	bool labelFollowsBox() const override { return !isAmbient() && Object::labelFollowsBox(); }

	// The canvas's name is bold whether or not anything has been drawn on it:
	// it is the name of the world everything here is in, and an empty world
	// is still that world. Every other node earns its bold by holding
	// something.
	void refreshLabelWeight(int contained) override
	{ Node::refreshLabelWeight(isAmbient() ? qMax(1, contained) : contained); }

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
