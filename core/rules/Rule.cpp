#include "core/rules/Rule.h"

#include <QFileInfo>
#include <QSet>
#include <algorithm>

#include "art/DiagramScene.h"
#include "art/Category.h"
#include "art/Arrow.h"
#include "core/io/SceneFile.h"
#include "core/rules/Pattern.h"   // Pattern::namesABuiltIn: one rule for both matchers

// ---------------------------------------------------------------- reading a rule

Rule::Rule(const QString& path)
	: m_path(path)
{
	auto scene = std::make_unique<DiagramScene>();
	QString error;
	if (!SceneFile::load(scene.get(), path, &error))
		return;
	m_scene = std::move(scene);
	m_name = m_scene->statementName().isEmpty() ? QFileInfo(path).completeBaseName() : m_scene->statementName();
	extract();
}

Rule::~Rule() = default;

bool Rule::isConstant(const QString& label)
{
	return label == QLatin1String("0") || label == QLatin1String("1");
}

namespace
{
	// is any ancestor of this node, up to and excluding the root, dotted?
	bool underADottedThing(Node* node, Category* root)
	{
		for (QGraphicsItem* p = node->parentItem(); p != nullptr && p != root; p = p->parentItem())
			if (auto* up = dynamic_cast<Node*>(p); up != nullptr && up->existsSuch())
				return true;
		return false;
	}

	// is any ancestor of this node, up to and excluding the root, crossed out?
	bool underAStruckThing(Node* node, Category* root)
	{
		for (QGraphicsItem* p = node->parentItem(); p != nullptr && p != root; p = p->parentItem())
			if (auto* up = dynamic_cast<Node*>(p); up != nullptr && up->markedForDeletion())
				return true;
		return false;
	}

	int depthOf(Node* node)
	{
		int depth = 0;
		for (QGraphicsItem* p = node->parentItem(); p != nullptr; p = p->parentItem())
			++depth;
		return depth;
	}

	void collect(QGraphicsItem* parent, QList<Node*>& out)
	{
		for (QGraphicsItem* child : parent->childItems())
			if (auto* node = dynamic_cast<Node*>(child))
			{
				out << node;
				collect(node, out);
			}
	}
}

void Rule::extract()
{
	m_root = m_scene->ambientCategory();
	if (m_root == nullptr)
		return;

	QList<Node*> everything;
	collect(m_root, everything);

	// Solid is premise, dotted (or inside something dotted) is conclusion.
	// Crossed out in red is BOTH a premise - it has to be found - and what the
	// rule takes away, so it is filed in the premise and noted again as one to
	// go. A thing inside something struck off goes with it, so only the
	// outermost mark needs listing.
	for (Node* node : everything)
	{
		const bool claimed = node->existsSuch() || underADottedThing(node, m_root);
		if (auto* arrow = dynamic_cast<Arrow*>(node))
			(claimed ? m_conclusionArrows : m_premiseArrows) << arrow;
		else
			(claimed ? m_conclusionObjects : m_premiseObjects) << node;

		if (!claimed && node->markedForDeletion() && !underAStruckThing(node, m_root))
		{
			if (auto* arrow = dynamic_cast<Arrow*>(node))
				m_deletedArrows << arrow;
			else
				m_deletedObjects << node;
		}
	}

	promoteTermsOverExistentials();

	// parents before children, so a parent is always bound before its child
	// is looked for
	auto byDepth = [](Node* a, Node* b) { return depthOf(a) < depthOf(b); };
	std::sort(m_premiseObjects.begin(), m_premiseObjects.end(), byDepth);
	std::sort(m_conclusionObjects.begin(), m_conclusionObjects.end(), byDepth);
}

// NOTHING YOU MUST FIND CAN BE SPELT OUT OF SOMETHING THAT DOES NOT EXIST YET.
//
// "For every group G and element x there is a y with xy^{-1} = 1 = y^{-1}x."
// Drawn, that is a solid x, a dashed y, and the terms xy^{-1} and y^{-1}x -
// and those two terms are, on the face of it, drawn solid, because nobody
// dashes every letter of a formula. Read literally that makes them part of
// the PREMISE: the rule would go looking for a diagram in which xy^{-1} has
// already been written down, and the axiom would fire nowhere, because the
// whole point of it is that you have not got a y yet.
//
// A term that mentions y is not something you can be asked to find. It comes
// into existence when y does, so it belongs on the same side of the
// implication - which is what a person means by writing it there and what
// "juxtaposition is defined elsewhere" takes for granted.
//
// So: any premise node whose label uses the name of something the rule merely
// CLAIMS, as a whole name, is moved over to the conclusion; and so is anything
// drawn inside it, and any arrow with an end that has moved, which cannot be
// looked for once one of its ends is no longer there to look for. Run to a
// fixed point, because a term can be spelt out of a term - z = xy^{-1} moves
// because xy^{-1} did.
//
// This changes nothing for a rule that does not name its existentials in its
// premise, which is nearly all of them.
void Rule::promoteTermsOverExistentials()
{
	auto namesOf = [](const QList<Node*>& objects, const QList<Arrow*>& arrows) {
		QStringList names;
		for (Node* node : objects)
			if (const QString id = node->id(); !id.isEmpty() && !Rule::isConstant(id))
				names << id;
		for (Arrow* arrow : arrows)
			if (const QString id = arrow->id(); !id.isEmpty() && !Rule::isConstant(id))
				names << id;
		return names;
	};

	// Every label drawn in this rule, so that a name sitting against another
	// name can be told from a name sitting inside a longer word: xy^{-1}
	// mentions y because x is one of these, and Hom would not mention o
	// unless H were (see Node::labelMentions).
	QStringList everyName;
	for (const QList<Node*>* list : { &m_premiseObjects, &m_conclusionObjects })
		for (Node* node : *list)
			if (const QString id = node->id(); !id.isEmpty() && !everyName.contains(id))
				everyName << id;
	for (const QList<Arrow*>* list : { &m_premiseArrows, &m_conclusionArrows })
		for (Arrow* arrow : *list)
			if (const QString id = arrow->id(); !id.isEmpty() && !everyName.contains(id))
				everyName << id;

	for (bool moved = true; moved; )
	{
		moved = false;
		const QStringList claimed = namesOf(m_conclusionObjects, m_conclusionArrows);
		if (claimed.isEmpty())
			return;

		// the objects first: an arrow can only follow an end that has gone
		for (int i = m_premiseObjects.size() - 1; i >= 0; --i)
		{
			Node* node = m_premiseObjects.at(i);
			const QString id = node->id();
			bool spelt = false;
			for (const QString& name : claimed)
				if (Node::labelMentions(id, name, everyName))
				{
					spelt = true;
					break;
				}
			// and anything drawn inside something that has moved goes with it
			if (!spelt)
				for (Node* gone : m_conclusionObjects)
					if (node->parentItem() == gone)
					{
						spelt = true;
						break;
					}
			if (!spelt)
				continue;
			m_premiseObjects.removeAt(i);
			m_conclusionObjects << node;
			m_deletedObjects.removeAll(node);
			moved = true;
		}

		for (int i = m_premiseArrows.size() - 1; i >= 0; --i)
		{
			Arrow* arrow = m_premiseArrows.at(i);
			const bool endGone = m_conclusionObjects.contains(arrow->domain())
			                  || m_conclusionObjects.contains(arrow->codomain());
			bool spelt = endGone;
			if (!spelt)
				for (const QString& name : claimed)
					if (Node::labelMentions(arrow->id(), name, everyName))
					{
						spelt = true;
						break;
					}
			if (!spelt)
				continue;
			m_premiseArrows.removeAt(i);
			m_conclusionArrows << arrow;
			m_deletedArrows.removeAll(arrow);
			moved = true;
		}
	}
}

QHash<QString, QString> RuleMatch::bindings() const
{
	QHash<QString, QString> map;
	for (auto it = objects.constBegin(); it != objects.constEnd(); ++it)
		if (!it.key()->id().isEmpty())
			map.insert(it.key()->id(), it.value()->id());
	for (auto it = arrows.constBegin(); it != arrows.constEnd(); ++it)
		if (!it.key()->id().isEmpty())
			map.insert(it.key()->id(), it.value()->id());
	return map;
}

// ---------------------------------------------------------------- finding it

namespace
{
	// The objects drawn directly in a node: arrows are not among them.
	QList<Node*> objectsIn(QGraphicsItem* parent)
	{
		QList<Node*> objects;
		for (QGraphicsItem* child : parent->childItems())
			if (auto* node = dynamic_cast<Node*>(child); node != nullptr && dynamic_cast<Arrow*>(node) == nullptr)
				objects << node;
		return objects;
	}

	QList<Arrow*> arrowsIn(QGraphicsItem* parent)
	{
		QList<Arrow*> arrows;
		for (QGraphicsItem* child : parent->childItems())
			if (auto* arrow = dynamic_cast<Arrow*>(child))
				arrows << arrow;
		return arrows;
	}

	// the pattern node this one sits directly inside: the root, or a premise
	// object
	Node* patternParentOf(Node* node)
	{
		return dynamic_cast<Node*>(node->parentItem());
	}

	struct Search
	{
		const Rule& rule;
		DiagramScene* diagram;
		int cap;
		int budget = 200000;   // steps of search, whatever they find
		QList<RuleMatch> found;
		RuleMatch current;
		QSet<Node*> usedObjects;
		QSet<Arrow*> usedArrows;

		Search(const Rule& r, DiagramScene* d, int c) : rule(r), diagram(d), cap(c) {}

		// What the RULE asserts of the piece its pattern sits in, the diagram
		// must assert too. A rule drawn with exact rows is about exact rows: it
		// has no business firing where nothing of the kind has been claimed.
		// One way only - a diagram may say MORE than the rule asks of it.
		bool claimsAgree(Node* pattern, Node* target) const
		{
			if (pattern->rowsExactInComponent() && !target->rowsExactInComponent())
				return false;
			if (pattern->columnsExactInComponent() && !target->columnsExactInComponent())
				return false;
			return true;
		}

		bool labelsAgree(Node* pattern, Node* target) const
		{
			// a constant must be met by the same constant; a variable meets
			// anything, but a constant in the diagram is not a variable's to
			// take unless the pattern is that constant too
			const QString p = pattern->id();
			const QString t = target->id();
			if (Rule::isConstant(p))
				return t == p && claimsAgree(pattern, target);
			return !Rule::isConstant(t) && claimsAgree(pattern, target);
		}

		bool arrowLabelsAgree(Arrow* pattern, Arrow* target) const
		{
			const QString p = pattern->effectiveId();
			const QString t = target->effectiveId();
			if (Rule::isConstant(p))
				return t == p && claimsAgree(pattern, target);
			return !Rule::isConstant(t) && claimsAgree(pattern, target);
		}

		void finish()
		{
			RuleMatch match = current;
			for (Node* node : match.objects)
				if (node != diagram->ambientCategory())
					match.bounds |= node->sceneBoundingRect();
			for (Arrow* arrow : match.arrows)
				match.bounds |= arrow->sceneBoundingRect();
			if (match.bounds.isEmpty() && !match.objects.isEmpty())
				match.bounds = diagram->ambientCategory()->sceneBoundingRect();
			found << match;
		}

		// the arrows, once every object is placed
		void matchArrows(int index)
		{
			if (--budget < 0 || found.size() >= cap)
				return;
			const QList<Arrow*>& arrows = rule.premiseArrows();
			if (index >= arrows.size())
			{
				finish();
				return;
			}
			Arrow* pattern = arrows.at(index);
			Node* from = current.objects.value(pattern->domain());
			Node* to = current.objects.value(pattern->codomain());
			Node* home = current.objects.value(patternParentOf(pattern));
			if (from == nullptr || to == nullptr || home == nullptr)
				return;   // an end outside the pattern: this rule cannot be matched
			for (Arrow* candidate : arrowsIn(home))
			{
				if (usedArrows.contains(candidate))
					continue;
				if (candidate->domain() != from || candidate->codomain() != to)
					continue;
				if (!arrowLabelsAgree(pattern, candidate))
					continue;
				usedArrows.insert(candidate);
				current.arrows.insert(pattern, candidate);
				matchArrows(index + 1);
				current.arrows.remove(pattern);
				usedArrows.remove(candidate);
				if (found.size() >= cap)
					return;
			}
		}

		// the objects, parents first
		void matchObjects(int index)
		{
			if (--budget < 0 || found.size() >= cap)
				return;
			const QList<Node*>& objects = rule.premiseObjects();
			if (index >= objects.size())
			{
				matchArrows(0);
				return;
			}
			Node* pattern = objects.at(index);
			Node* patternParent = patternParentOf(pattern);
			Node* home = current.objects.value(patternParent);
			if (home == nullptr)
				return;   // its parent was not placed: cannot be
			for (Node* candidate : objectsIn(home))
			{
				if (usedObjects.contains(candidate) || candidate == diagram->ambientCategory())
					continue;
				if (!labelsAgree(pattern, candidate))
					continue;
				usedObjects.insert(candidate);
				current.objects.insert(pattern, candidate);
				matchObjects(index + 1);
				current.objects.remove(pattern);
				usedObjects.remove(candidate);
				if (found.size() >= cap)
					return;
			}
		}
	};
}

QList<RuleMatch> RuleMatcher::find(const Rule& rule, DiagramScene* diagram, int cap)
{
	QList<RuleMatch> none;
	if (!rule.isValid() || rule.root() == nullptr || diagram == nullptr || diagram->ambientCategory() == nullptr)
		return none;

	// WHAT THE CATEGORY A RULE IS DRAWN IN STANDS FOR.
	//
	// A rule drawn in R-Mod is about R-Mod: it may use that modules have
	// kernels, that 0 is an object, that a sum of maps is a map. It fires
	// wherever R-Mod is drawn and nowhere else.
	//
	// A rule drawn in a category the user simply called C is not about any
	// category in particular. C is a VARIABLE, exactly as X and f are
	// variables inside it - "for any category C, and any composable f and g
	// in it..." - and the whole point of drawing it that way is that it holds
	// of Top, of Set, of a category drawn inside another one, of anything.
	// Matching it by NAME meant it held of categories that happened to be
	// called C and of nothing else, which is not what anybody draws.
	//
	// What a variable category does carry is the structure it was drawn WITH:
	// a rule drawn in a category ticked as additive is a rule about additive
	// categories, and has no business firing in one that has not been said to
	// be. Same direction as claimsAgree - the diagram may say more than the
	// rule asks, never less.
	Category* pattern = rule.root();
	const QString named = pattern->id();
	// By NAME, not by class. A rule for any category is drawn by taking a
	// fresh scene and calling it C - and a fresh scene starts in BigCat, so
	// what is on the canvas is a BigCat wearing the name C. Asking the class
	// would make that rule about BigCat and nothing else, which is the
	// opposite of what writing C meant.
	const bool aboutOne = Pattern::namesABuiltIn(named);
	const QStringList wanted = pattern->properties();

	auto standsFor = [&](Category* candidate) {
		if (candidate == nullptr)
			return false;
		if (aboutOne)
			return candidate->id() == named || candidate->builtInName() == named;
		// a variable category: any at all, carrying whatever structure was
		// ticked on it BY HAND. A built-in renamed to a letter carries its
		// class's structure, which is not a claim anybody made about C.
		if (pattern->builtInName().isEmpty())
			for (const QString& key : wanted)
				if (!candidate->has(key))
					return false;
		return true;
	};

	QList<Node*> roots;
	if (standsFor(diagram->ambientCategory()))
		roots << diagram->ambientCategory();
	for (Node* node : diagram->labelledNodes())
		if (auto* category = dynamic_cast<Category*>(node); standsFor(category))
			roots << category;

	Search search(rule, diagram, cap);
	for (Node* root : roots)
	{
		search.current.objects.insert(rule.root(), root);
		search.matchObjects(0);
		search.current.objects.remove(rule.root());
		if (search.found.size() >= cap)
			break;
	}
	return search.found;
}

// ---------------------------------------------------------------- applying it

namespace
{
	bool isNameChar(QChar c)
	{
		return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QChar(0x2032)
		    || (c.unicode() >= 0x2080 && c.unicode() <= 0x2089);
	}

	// A conclusion label with the match's substitution made: every whole
	// variable name that the match bound is replaced by what it stands for.
	// "Ker f" with f standing for k becomes "Ker k"; a constant stays.
	// A derived label with its sources filled in - the same reading
	// Node::refreshDerivedLabel makes, worked out BEFORE the node exists so
	// that the name it is going to have can be compared against what is
	// already drawn.
	QString filledIn(const QString& pattern, const QList<Node*>& sources)
	{
		QString text = pattern;
		for (int i = 0; i < sources.size(); ++i)
			text.replace("%" + QString::number(i + 1),
			             sources.at(i) == nullptr ? QStringLiteral("?") : sources.at(i)->id());
		return text;
	}

	// An arrow already drawn from `from` to `to` and going by `name`, or
	// nullptr. Read by effectiveId so that an unlabelled arrow is compared by
	// what its category CALLS an unlabelled arrow, exactly as the matcher
	// compares them.
	Arrow* arrowBetween(Category* into, Node* from, Node* to, const QString& name)
	{
		// `into` and not from->parentItem(): the arrow being considered is
		// about to be drawn as a child of the category, which is where any
		// arrow already running between these two would be as well.
		if (into == nullptr || from == nullptr || to == nullptr || name.isEmpty())
			return nullptr;
		for (QGraphicsItem* child : into->childItems())
			if (auto* arrow = dynamic_cast<Arrow*>(child))
				if (arrow->domain() == from && arrow->codomain() == to && arrow->effectiveId() == name)
					return arrow;
		return nullptr;
	}

	QString substituted(const QString& label, const QHash<QString, QString>& bindings)
	{
		if (label.isEmpty() || Rule::isConstant(label))
			return label;
		QList<QString> variables = bindings.keys();
		std::sort(variables.begin(), variables.end(), [](const QString& a, const QString& b) {
			return a.size() > b.size();   // longest first, so "AB" is not read as A then B
		});
		QString out;
		int at = 0;
		while (at < label.size())
		{
			bool done = false;
			for (const QString& variable : variables)
			{
				if (variable.isEmpty() || !QStringView(label).mid(at).startsWith(variable))
					continue;
				const bool leftClear = at == 0 || !isNameChar(label.at(at - 1));
				const bool rightClear = at + variable.size() >= label.size() || !isNameChar(label.at(at + variable.size()));
				if (!leftClear || !rightClear)
					continue;
				out += bindings.value(variable);
				at += variable.size();
				done = true;
				break;
			}
			if (!done)
				out += label.at(at++);
		}
		return out;
	}
}

QList<Node*> RuleMatcher::apply(const Rule& rule, const RuleMatch& match, DiagramScene* diagram)
{
	QList<Node*> made;
	if (!rule.isValid() || diagram == nullptr)
		return made;

	const QHash<QString, QString> bindings = match.bindings();
	QHash<Node*, Node*> placed = match.objects;   // pattern -> diagram, growing as we draw

	// ...AND THE ARROWS THE MATCH BOUND.
	//
	// A conclusion's label may be built out of premise ARROWS - a composite is
	// named "%1%2" over the two it composes - and those names have to be looked
	// up the same way an object's is. With only the objects in here, a
	// composite's sources resolved to nothing, setDerivedLabel was skipped, and
	// the arrow kept the name the RULE was written with: gf, whatever the two
	// arrows it actually matched were called.
	//
	// The plain-name path cannot save it either: substituted() replaces whole
	// names only, and in "gf" neither letter stands alone.
	for (auto it = match.arrows.constBegin(); it != match.arrows.constEnd(); ++it)
		placed.insert(it.key(), it.value());

	// the conclusion's objects, parents first, each inside what its pattern
	// parent stands for
	for (Node* pattern : rule.conclusionObjects())
	{
		Node* home = placed.value(patternParentOf(pattern));
		auto* into = dynamic_cast<Category*>(home);
		if (into == nullptr)
			continue;   // nowhere to put it
		QString name = substituted(pattern->id(), bindings);
		if (!Rule::isConstant(name) && into->nameInUse(name))
		{
			// the rule's own name for what it makes is taken here: a prime
			// keeps it apart, as many as it takes
			QString fresh = name;
			for (int guard = 0; guard < 26 && into->nameInUse(fresh); ++guard)
				fresh += QChar(0x2032);
			name = fresh;
		}
		// the same spot inside its home as in the rule, on this diagram's grid
		Object* object = into->createObject(name, into->mapToScene(pattern->pos()));
		if (!pattern->labelPattern().isEmpty())
		{
			// a label built out of others carries the formula over, tied to
			// what those others stand for here
			QList<Node*> sources;
			for (Node* source : pattern->labelSources())
				if (Node* bound = placed.value(source))
					sources << bound;
			if (!sources.isEmpty())
				object->setDerivedLabel(pattern->labelPattern(), sources);
		}
		placed.insert(pattern, object);
		made << object;
	}

	// and its arrows, between what their ends stand for
	for (Arrow* pattern : rule.conclusionArrows())
	{
		Node* from = placed.value(pattern->domain());
		Node* to = placed.value(pattern->codomain());
		auto* into = dynamic_cast<Category*>(placed.value(patternParentOf(pattern)));
		if (from == nullptr || to == nullptr || into == nullptr)
			continue;

		// WHAT THIS ARROW IS TO BE CALLED, decided BEFORE anything is drawn.
		//
		// A label built out of other labels is not a name the rule chose: it
		// is "whatever those turn out to be called here", and it has the last
		// word. Working the plain name out first and priming it - f becomes
		// f' because the diagram already has an f - and only then applying the
		// formula meant the prime was either wasted or, when the formula
		// failed to resolve, left standing as the name. That is where f' and
		// f'' came from.
		QList<Node*> sources;
		for (Node* source : pattern->labelSources())
			if (Node* bound = placed.value(source))
				sources << bound;
		const bool derived = !pattern->labelPattern().isEmpty()
		                  && !sources.isEmpty()
		                  && sources.size() == pattern->labelSources().size();

		QString name = substituted(pattern->id(), bindings);
		// A name is only primed when it is this rule's own choice of name. A
		// derived one is about to be overwritten, and a prime on it would be
		// a prime on a name nobody asked for.
		if (!derived && !name.isEmpty() && !Rule::isConstant(name) && into->nameInUse(name))
		{
			QString fresh = name;
			for (int guard = 0; guard < 26 && into->nameInUse(fresh); ++guard)
				fresh += QChar(0x2032);
			name = fresh;
		}

		// ALREADY THERE IS ALREADY THERE.
		//
		// The conclusion says there EXISTS an arrow of this name between these
		// two. If the diagram already has one, the claim is made good and
		// there is nothing to draw: drawing a second would assert a second,
		// DIFFERENT morphism, which the rule does not say. Applying the same
		// rule twice used to pile parallel copies on top of each other.
		const QString wanted = derived ? filledIn(pattern->labelPattern(), sources) : name;
		if (Arrow* already = arrowBetween(into, from, to, wanted))
		{
			placed.insert(pattern, already);
			continue;
		}

		Arrow* arrow = into->createArrow(name, from, to);
		// NOT the rule's bends. A bend is a point in the arrow's own frame,
		// and that frame is the rule's category with the rule's objects at the
		// rule's spacing - none of which this diagram shares. Carried over
		// literally they pulled the line somewhere it had no business being
		// and stranded the label out on its own with no line under it.
		if (derived)
			arrow->setDerivedLabel(pattern->labelPattern(), sources);
		placed.insert(pattern, arrow);
		made << arrow;
	}

	// what the rule said there is, there now is: drawn solid, never dotted -
	// the claim has been made good
	for (Node* node : made)
	{
		node->setExistsSuch(false);
		node->setDeleteMark(false);   // the mark is the RULE's, not the diagram's
	}

	// One step in the history, whatever it amounted to: which rule, and what
	// its variables stood for here. A rule that draws nothing still happened -
	// citing an axiom is a step of a proof.
	QStringList variables, values;
	QStringList where;
	for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it)
	{
		variables << it.key();
		values << it.value();
		if (it.key() != it.value())
			where << QString("%1 = %2").arg(it.key(), it.value());
	}
	std::sort(where.begin(), where.end());   // a QHash has no order of its own

	// what the red crosses stood for here: found, and now taken out
	QList<Node*> doomed;
	for (Node* pattern : rule.deletedObjects())
		if (Node* bound = match.objects.value(pattern); bound != nullptr && bound != diagram->ambientCategory())
			doomed << bound;
	for (Arrow* pattern : rule.deletedArrows())
		if (Arrow* bound = match.arrows.value(pattern))
			doomed << bound;
	// anything just drawn in is the rule's own work and does not go
	for (Node* node : made)
		doomed.removeAll(node);

	QStringList goneNames;
	for (Node* node : doomed)
		goneNames << (node->id().isEmpty() ? QStringLiteral("an unnamed one") : node->id());

	QString what = QString("Applied %1").arg(rule.name());
	if (!where.isEmpty())
		what += QString(" with %1").arg(where.join(", "));
	if (!goneNames.isEmpty())
		what += QString(", taking out %1").arg(goneNames.join(", "));
	diagram->recordRuleApplication(what, made, rule.path(), rule.name(), variables, values);

	// The deletions go last, and as their own change: an arrow the rule drew
	// may end on something that is about to be taken away, and it has to be
	// there to be carried off with it.
	if (!doomed.isEmpty())
		diagram->deleteNodes(doomed);

	return made;
}
