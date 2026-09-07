#include "Constructions.h"

#include <QMenu>
#include <QSet>
#include "../Category.h"
#include "../Arrow.h"
#include "../DiagramScene.h"
#include "../TutorSession.h"

// ---------------------------------------------------------------- the base


QAction* CategoryConstruction::addTeachAction(QMenu& menu, Category* category, const QString& title)
{
	QAction* act = menu.addAction(title);
	QObject::connect(act, &QAction::triggered, this, [this, category] {
		if (auto* scene = dynamic_cast<DiagramScene*>(category->scene()))
			teach(scene);
	});
	return act;
}

void CategoryConstruction::addConstructions(Category* category, const MenuGroup& group)
{
	QMenu* where = group(menuSection());
	if (where != nullptr)
		addTeachAction(*where, category, menuTitle())->setToolTip(description());
}
QString CategoryConstruction::subscript(int n)
{
	QString s;
	for (QChar c : QString::number(n))
		s += QChar(0x2080 + (c.unicode() - '0'));
	return s;
}

QString CategoryConstruction::names(const QList<Node*>& picks, const QString& sep)
{
	QStringList ids;
	for (Node* n : picks)
		ids << n->id();
	return ids.join(sep);
}

QString CategoryConstruction::pattern(const QList<Node*>& picks, const QString& sep)
{
	QStringList parts;
	for (int i = 0; i < picks.size(); ++i)
		parts << ("%" + QString::number(i + 1));
	return parts.join(sep);
}

QPointF CategoryConstruction::along(Node* node)
{
	auto* arrow = dynamic_cast<Arrow*>(node);
	if (arrow == nullptr || arrow->domain() == nullptr || arrow->codomain() == nullptr)
		return QPointF(1, 0);
	QPointF direction = arrow->codomain()->pos() - arrow->domain()->pos();
	const qreal length = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
	if (length < 1e-6)
		return QPointF(1, 0);   // both ends in the same place: anywhere will do
	return direction / length;
}

QPointF CategoryConstruction::across(Node* node)
{
	const QPointF direction = along(node);
	return QPointF(-direction.y(), direction.x());
}

QPointF CategoryConstruction::centroid(const QList<Node*>& picks)
{
	QPointF c;
	for (Node* n : picks)
		c += n->pos();
	return picks.isEmpty() ? c : c / picks.size();
}

Object* CategoryConstruction::place(const QString& name, const QPointF& categoryPos)
{
	return category()->createObject(name, category()->mapToScene(categoryPos));
}

Arrow* CategoryConstruction::arrow(const QString& name, Node* from, Node* to)
{
	// Through the category's own factory, never `new Arrow`. The factory is
	// what decides the KIND of arrow this is - a functor in BigCat - and what
	// gives it the property that carries elements from its domain to its
	// codomain. Building one by hand produced an arrow with neither, which is
	// why a constructed kernel arrow had no Map option while a drawn one did.
	return category()->createArrow(name, from, to);
}

void CategoryConstruction::onBegin(TutorSession& s)
{
	s.say(instruction(), category());
}

bool CategoryConstruction::onPick(TutorSession& s, Node* node)
{
	const bool isArrow = dynamic_cast<Arrow*>(node) != nullptr;
	if (node == nullptr || node->parentItem() != category())
	{
		s.say("That is not in " + category()->id() + ". Pick " + (picksArrows() ? "arrows" : "objects") + " drawn inside it.", category());
		return false;
	}
	if (isArrow != picksArrows())
	{
		s.say(picksArrows() ? "That is an object; this construction wants arrows." : "That is an arrow; this construction wants objects.", node);
		return false;
	}
	if (s.picks().contains(node))
		return false;
	if (s.picks().size() >= maxPicks())
	{
		s.say("That is enough: press Done.", category());
		return false;
	}
	const QString why = vet(s.picks(), node);
	if (!why.isEmpty())
	{
		s.say(why, node);
		return false;
	}
	const int have = s.picks().size() + 1;
	QString remark = "Chosen so far: " + names(s.picks(), ", ") + (s.picks().isEmpty() ? "" : ", ") + node->id() + ". ";
	remark += have >= maxPicks() ? "Press Done." : (have >= minPicks() ? "Pick the next one, or press Done." : "Pick the next one.");
	s.say(remark, node);
	return true;
}

bool CategoryConstruction::onDone(TutorSession& s)
{
	if (s.picks().size() < minPicks())
	{
		s.say(QString("This needs at least %1 %2: %3 chosen so far.").arg(minPicks()).arg(picksArrows() ? "arrows" : "objects").arg(s.picks().size()), category());
		return false;
	}

	// whatever the construction draws is one step of the diagram's history:
	// take note of what is there before and after, so every construction is
	// recorded without each one having to remember to do it
	QSet<QGraphicsItem*> before;
	for (QGraphicsItem* child : category()->childItems())
		before.insert(child);

	if (!build(s))
		return false;

	QList<Node*> made;
	for (QGraphicsItem* child : category()->childItems())
		if (!before.contains(child))
			if (auto* node = dynamic_cast<Node*>(child))
				made << node;
	if (auto* scene = dynamic_cast<DiagramScene*>(category()->scene()))
		scene->recordCreation(QString("%1 in %2").arg(tutorTitle(), category()->id()), made);
	return true;
}

// ---------------------------------------------------------------- products, coproducts, biproducts

bool HasProducts::build(TutorSession& s)
{
	const QList<Node*>& picks = s.picks();
	const QString sep = QString(" ") + QChar(0x00D7) + " ";
	Object* product = place(names(picks, sep), centroid(picks) + QPointF(0, 110));
	product->setDerivedLabel(pattern(picks, sep), picks);   // A x B follows A and B
	for (int i = 0; i < picks.size(); ++i)
		arrow("p" + subscript(i + 1), product, picks[i]);
	return true;
}

bool HasCoproducts::build(TutorSession& s)
{
	const QList<Node*>& picks = s.picks();
	Object* sum = place(names(picks, " + "), centroid(picks) + QPointF(0, 110));
	sum->setDerivedLabel(pattern(picks, " + "), picks);
	for (int i = 0; i < picks.size(); ++i)
		arrow("i" + subscript(i + 1), picks[i], sum);
	return true;
}

bool IsAdditive::build(TutorSession& s)
{
	const QList<Node*>& picks = s.picks();
	const QString sep = QString(" ") + QChar(0x2295) + " ";
	Object* bi = place(names(picks, sep), centroid(picks) + QPointF(0, 110));
	bi->setDerivedLabel(pattern(picks, sep), picks);
	for (int i = 0; i < picks.size(); ++i)
	{
		arrow("p" + subscript(i + 1), bi, picks[i]);
		arrow("i" + subscript(i + 1), picks[i], bi);
	}
	return true;
}

// ---------------------------------------------------------------- equalizers, coequalizers

namespace
{
	QString notParallel(const QList<Node*>& picks, Node* candidate)
	{
		if (picks.isEmpty())
			return QString();
		auto* f = dynamic_cast<Arrow*>(picks.first());
		auto* g = dynamic_cast<Arrow*>(candidate);
		if (f == nullptr || g == nullptr)
			return QString();
		if (f->domain() != g->domain() || f->codomain() != g->codomain())
			return "Not parallel to " + f->id() + ": both arrows must share the same domain and codomain.";
		return QString();
	}
}

QString HasEqualizers::vet(const QList<Node*>& picks, Node* candidate) const
{
	return notParallel(picks, candidate);
}

bool HasEqualizers::build(TutorSession& s)
{
	auto* f = dynamic_cast<Arrow*>(s.picks()[0]);
	auto* g = dynamic_cast<Arrow*>(s.picks()[1]);
	Node* a = f->domain();
	Object* eq = place("Eq(" + f->id() + ", " + g->id() + ")", a->pos() - along(f) * reach());
	eq->setDerivedLabel("Eq(%1, %2)", { f, g });
	arrow("e", eq, a);
	return true;
}

QString HasCoequalizers::vet(const QList<Node*>& picks, Node* candidate) const
{
	return notParallel(picks, candidate);
}

bool HasCoequalizers::build(TutorSession& s)
{
	auto* f = dynamic_cast<Arrow*>(s.picks()[0]);
	auto* g = dynamic_cast<Arrow*>(s.picks()[1]);
	Node* b = f->codomain();
	Object* coeq = place("Coeq(" + f->id() + ", " + g->id() + ")", b->pos() + along(f) * reach());
	coeq->setDerivedLabel("Coeq(%1, %2)", { f, g });
	arrow("q", b, coeq);
	return true;
}

// ---------------------------------------------------------------- zero object


void HasZeroObject::addConstructions(Category* category, const MenuGroup& group)
{
	// nothing to pick: it goes in at once, above the middle of the category
	QMenu* where = group(menuSection());
	if (where == nullptr)
		return;
	QAction* act = where->addAction(menuTitle());
	act->setToolTip(description());
	QObject::connect(act, &QAction::triggered, this, [category] {
		category->createObject("0", category->mapToScene(QPointF(0, -80)));
	});
}
bool HasZeroObject::build(TutorSession&)
{
	place("0", QPointF(0, -80));
	return true;
}

// ---------------------------------------------------------------- kernels, cokernels, images


void HasKernels::addConstructions(Category* category, const MenuGroup& group)
{
	if (QMenu* limits = group("Limits"))
	{
		QAction* ker = limits->addAction("Kernel of an arrow...");
		ker->setToolTip("The kernel of a morphism, with its arrow into the domain.");
		QObject::connect(ker, &QAction::triggered, this, [this, category] {
			m_cokernel = false;
			if (auto* scene = dynamic_cast<DiagramScene*>(category->scene())) teach(scene);
		});
	}
	if (QMenu* colimits = group("Colimits"))
	{
		QAction* coker = colimits->addAction("Cokernel of an arrow...");
		coker->setToolTip("The cokernel of a morphism, with its arrow out of the codomain.");
		QObject::connect(coker, &QAction::triggered, this, [this, category] {
			m_cokernel = true;
			if (auto* scene = dynamic_cast<DiagramScene*>(category->scene())) teach(scene);
		});
	}
}
bool HasKernels::build(TutorSession& s)
{
	auto* f = dynamic_cast<Arrow*>(s.picks()[0]);
	if (m_cokernel)
	{
		Node* b = f->codomain();
		// on past the codomain, the way the arrow was already going
		Object* coker = place("Coker " + f->id(), b->pos() + along(f) * reach());
		coker->setDerivedLabel("Coker %1", { f });
		arrow("c", b, coker);
	}
	else
	{
		Node* a = f->domain();
		// back up the arrow from its domain: the kernel of a map pointing down
		// belongs above it
		Object* ker = place("Ker " + f->id(), a->pos() - along(f) * reach());
		ker->setDerivedLabel("Ker %1", { f });
		arrow("k", ker, a);
	}
	return true;
}

bool IsAbelian::build(TutorSession& s)
{
	auto* f = dynamic_cast<Arrow*>(s.picks()[0]);
	Node* a = f->domain();
	Node* b = f->codomain();
	// beside the arrow, clear of its line, whichever way it runs
	Object* im = place("Im " + f->id(), (a->pos() + b->pos()) / 2 + across(f) * 100);
	im->setDerivedLabel("Im %1", { f });
	arrow("e", a, im);
	arrow("m", im, b);
	return true;
}
