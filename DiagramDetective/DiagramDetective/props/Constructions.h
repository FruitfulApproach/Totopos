#pragma once

#include <QList>
#include <QPointF>
#include "CategoryProp.h"
#include "../Tutor.h"

class Arrow;
class Object;
class QAction;

// A property that OFFERS A CONSTRUCTION from the category's menu and teaches
// it: pick some objects (or arrows) in order, press Done, and the
// construction appears with its structure arrows. Subclasses say what to
// pick and how to build.
class CategoryConstruction : public CategoryProp, public Tutor
{
	Q_OBJECT

public:
	explicit CategoryConstruction(Category* category = nullptr) : CategoryProp(category) {}

	void addConstructions(Category* category, const MenuGroup& group) override;
	QString tutorTitle() const override { return menuTitle(); }

protected:
	// which part of the Construct menu this belongs in: "Limits", "Colimits",
	// "Additive", or "" for Construct itself
	virtual QString menuSection() const = 0;
	virtual QString menuTitle() const = 0;     // "Product..."
	// hang one action for this construction in `menu`
	QAction* addTeachAction(QMenu& menu, Category* category, const QString& title);
	virtual QString instruction() const = 0;   // the first remark
	virtual bool picksArrows() const { return false; }
	virtual int minPicks() const { return 2; }
	virtual int maxPicks() const { return 1000; }
	// a further check on a candidate given the picks so far: empty when fine,
	// else the remark explaining the refusal
	virtual QString vet(const QList<Node*>& picks, Node* candidate) const { Q_UNUSED(picks); Q_UNUSED(candidate); return QString(); }
	// build it from the picks; false (with a remark said) keeps the session open
	virtual bool build(TutorSession& s) = 0;

	// helpers for build: an object of the category at a category-local point, an arrow in it
	Object* place(const QString& name, const QPointF& categoryPos);
	Arrow* arrow(const QString& name, Node* from, Node* to);
	static QPointF centroid(const QList<Node*>& picks);
	static QString subscript(int n);
	static QString names(const QList<Node*>& picks, const QString& sep);
	// "%1 x %2 x %3" over the picks, for Node::setDerivedLabel: the product
	// keeps naming its factors even after they are renamed
	static QString pattern(const QList<Node*>& picks, const QString& sep);

	// Which way an arrow points, as a unit vector in the category's own
	// coordinates, and the same turned a quarter. What is built FROM an arrow
	// is placed along its line rather than always to one side: the kernel of a
	// downward map belongs above its domain, not off to the left of it.
	static QPointF along(Node* arrow);
	static QPointF across(Node* arrow);
	// how far from the arrow's end a constructed object is placed
	static qreal reach() { return 150.0; }

	void onBegin(TutorSession& s) override;
	bool onPick(TutorSession& s, Node* node) override;
	bool onDone(TutorSession& s) override;
};

// A x B with projections p1, p2, ...
class HasProducts : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit HasProducts(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("hasProducts"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has products"); }
	QString description() const override { return QStringLiteral("Every pair of objects has a product A x B with its projections."); }
protected:
	QString menuSection() const override { return "Limits"; }
	QString menuTitle() const override { return "Product..."; }
	QString instruction() const override { return "Select the objects that go INTO the product, in order: click each one. Press Done (Enter) when they are all chosen; Esc cancels."; }
	bool build(TutorSession& s) override;
};

// A + B with injections i1, i2, ...
class HasCoproducts : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit HasCoproducts(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("hasCoproducts"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has coproducts"); }
	QString description() const override { return QStringLiteral("Every pair of objects has a coproduct A + B with its injections."); }
protected:
	QString menuSection() const override { return "Colimits"; }
	QString menuTitle() const override { return "Coproduct..."; }
	QString instruction() const override { return "Select the summands of the coproduct, in order: click each object. Press Done when they are all chosen."; }
	bool build(TutorSession& s) override;
};

// Eq(f, g) with e : Eq(f, g) -> A, for a parallel pair f, g : A -> B
class HasEqualizers : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit HasEqualizers(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("hasEqualizers"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has equalizers"); }
	QString description() const override { return QStringLiteral("Every parallel pair f, g : A -> B has an equalizer."); }
protected:
	QString menuSection() const override { return "Limits"; }
	QString menuTitle() const override { return "Equalizer..."; }
	QString instruction() const override { return "Click the two PARALLEL arrows f, g : A -> B to equalize, then press Done."; }
	bool picksArrows() const override { return true; }
	int minPicks() const override { return 2; }
	int maxPicks() const override { return 2; }
	QString vet(const QList<Node*>& picks, Node* candidate) const override;
	bool build(TutorSession& s) override;
};

// Coeq(f, g) with q : B -> Coeq(f, g)
class HasCoequalizers : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit HasCoequalizers(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("hasCoequalizers"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has coequalizers"); }
	QString description() const override { return QStringLiteral("Every parallel pair f, g : A -> B has a coequalizer."); }
protected:
	QString menuSection() const override { return "Colimits"; }
	QString menuTitle() const override { return "Coequalizer..."; }
	QString instruction() const override { return "Click the two PARALLEL arrows f, g : A -> B to coequalize, then press Done."; }
	bool picksArrows() const override { return true; }
	int minPicks() const override { return 2; }
	int maxPicks() const override { return 2; }
	QString vet(const QList<Node*>& picks, Node* candidate) const override;
	bool build(TutorSession& s) override;
};

// The zero object 0: placed directly, nothing to pick
class HasZeroObject : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit HasZeroObject(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("hasZeroObject"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has a zero object"); }
	QString description() const override { return QStringLiteral("An object 0 that is both initial and terminal."); }
	void addConstructions(Category* category, const MenuGroup& group) override;
protected:
	QString menuSection() const override { return QString(); }   // both a limit and a colimit
	QString menuTitle() const override { return "Zero object"; }
	QString instruction() const override { return QString(); }
	int minPicks() const override { return 0; }
	int maxPicks() const override { return 0; }
	bool build(TutorSession& s) override;
};

// Ker f with k : Ker f -> A, and Coker f with c : B -> Coker f
class HasKernels : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit HasKernels(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("hasKernels"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has kernels and cokernels"); }
	QString description() const override { return QStringLiteral("Every morphism has a kernel and a cokernel."); }
	// a kernel is a limit, a cokernel is a colimit: they go in different groups
	void addConstructions(Category* category, const MenuGroup& group) override;
protected:
	QString menuSection() const override { return "Limits"; }
	QString menuTitle() const override { return m_cokernel ? "Cokernel of an arrow..." : "Kernel of an arrow..."; }
	QString instruction() const override { return m_cokernel ? "Click the arrow whose cokernel you want, then press Done." : "Click the arrow whose kernel you want, then press Done."; }
	bool picksArrows() const override { return true; }
	int minPicks() const override { return 1; }
	int maxPicks() const override { return 1; }
	bool build(TutorSession& s) override;
private:
	bool m_cokernel = false;
};

// A (+) B with projections AND injections
class IsAdditive : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit IsAdditive(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("isAdditive"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Additive (biproducts, hom-groups)"); }
	QString description() const override { return QStringLiteral("Hom-sets are abelian groups, composition is bilinear, finite biproducts exist."); }
protected:
	QString menuSection() const override { return "Additive"; }
	QString menuTitle() const override { return "Biproduct..."; }
	QString instruction() const override { return "Select the objects of the biproduct, in order, then press Done: you get projections and injections."; }
	bool build(TutorSession& s) override;
};

// Im f with A -> Im f -> B
class IsAbelian : public CategoryConstruction
{
	Q_OBJECT
public:
	explicit IsAbelian(Category* category = nullptr) : CategoryConstruction(category) {}
	static QString Key() { return QStringLiteral("isAbelian"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Abelian"); }
	QString description() const override { return QStringLiteral("Additive, with kernels and cokernels, every mono a kernel and every epi a cokernel."); }
protected:
	QString menuSection() const override { return "Additive"; }
	QString menuTitle() const override { return "Image factorization..."; }
	QString instruction() const override { return "Click the arrow f : A -> B to factor through its image, then press Done."; }
	bool picksArrows() const override { return true; }
	int minPicks() const override { return 1; }
	int maxPicks() const override { return 1; }
	bool build(TutorSession& s) override;
};
