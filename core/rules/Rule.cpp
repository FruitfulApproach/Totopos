#include "core/rules/Rule.h"

#include <QFileInfo>
#include <QSet>
#include <algorithm>

#include "art/DiagramScene.h"
#include "art/Category.h"
#include "art/Arrow.h"
#include "core/io/SceneFile.h"

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

	// parents before children, so a parent is always bound before its child
	// is looked for
	auto byDepth = [](Node* a, Node* b) { return depthOf(a) < depthOf(b); };
	std::sort(m_premiseObjects.begin(), m_premiseObjects.end(), byDepth);
	std::sort(m_conclusionObjects.begin(), m_conclusionObjects.end(), byDepth);
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

	// The rule's own ambient category stands for a category of the SAME NAME
	// in the diagram: a rule about R-Mod is about R-Mod, wherever R-Mod is
	// drawn - the canvas itself, or a category drawn inside something.
	QList<Node*> roots;
	if (diagram->ambientCategory()->id() == rule.root()->id())
		roots << diagram->ambientCategory();
	for (Node* node : diagram->labelledNodes())
		if (auto* category = dynamic_cast<Category*>(node); category != nullptr && category->id() == rule.root()->id())
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
		QString name = substituted(pattern->id(), bindings);
		if (!name.isEmpty() && !Rule::isConstant(name) && into->nameInUse(name))
		{
			QString fresh = name;
			for (int guard = 0; guard < 26 && into->nameInUse(fresh); ++guard)
				fresh += QChar(0x2032);
			name = fresh;
		}
		Arrow* arrow = into->createArrow(name, from, to);
		arrow->setBends(pattern->bends());
		// A label built out of other labels carries the formula over, tied to
		// what those others stand for here - so a composite drawn as gf comes
		// out named after the two arrows it was actually matched against. The
		// same as for a conclusion object, and just as needed: without it a
		// rule's composite would keep the rule's own letters.
		if (!pattern->labelPattern().isEmpty())
		{
			QList<Node*> sources;
			for (Node* source : pattern->labelSources())
				if (Node* bound = placed.value(source))
					sources << bound;
			if (sources.size() == pattern->labelSources().size() && !sources.isEmpty())
				arrow->setDerivedLabel(pattern->labelPattern(), sources);
		}
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
