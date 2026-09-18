#include "core/rules/Pattern.h"

#include "core/rules/Rule.h"
#include "art/Node.h"
#include "art/Arrow.h"
#include "art/Category.h"

namespace
{
	const QList<int> kNothing;

	// everything the matcher wants to know about one live node, copied out
	PatternNode recordOf(Node* node)
	{
		PatternNode record;
		record.label = node->id();
		if (auto* arrow = dynamic_cast<Arrow*>(node))
		{
			record.isArrow = true;
			record.effectiveLabel = arrow->effectiveId();
		}
		else
		{
			record.effectiveLabel = record.label;
			record.isCategory = dynamic_cast<Category*>(node) != nullptr;
		}
		record.rowsExact = node->rowsExactInComponent();
		record.columnsExact = node->columnsExactInComponent();
		return record;
	}
}

void Pattern::index()
{
	for (int i = 0; i < m_nodes.size(); ++i)
	{
		const PatternNode& node = m_nodes.at(i);
		if (node.parent >= 0)
			(node.isArrow ? m_arrowChildren : m_objectChildren)[node.parent] << i;
		if (node.isCategory && !node.label.isEmpty())
			m_categoriesByLabel[node.label] << i;
	}
}

const QList<int>& Pattern::objectsIn(int parent) const
{
	const auto it = m_objectChildren.constFind(parent);
	return it == m_objectChildren.constEnd() ? kNothing : it.value();
}

const QList<int>& Pattern::arrowsIn(int parent) const
{
	const auto it = m_arrowChildren.constFind(parent);
	return it == m_arrowChildren.constEnd() ? kNothing : it.value();
}

QList<int> Pattern::rootsNamed(const QString& label) const
{
	QList<int> roots;
	if (m_nodes.isEmpty())
		return roots;
	// the ambient category itself, when it is the one the rule is about
	if (m_nodes.at(0).label == label)
		roots << 0;
	for (int i : m_categoriesByLabel.value(label))
		if (i != 0)
			roots << i;
	return roots;
}

Pattern Pattern::fromDiagram(Category* ambient, QList<Node*>* liveNodes)
{
	Pattern pattern;
	if (liveNodes != nullptr)
		liveNodes->clear();
	if (ambient == nullptr)
		return pattern;

	// A breadth of the whole diagram, parents before children so that a
	// node's parent index is always already assigned.
	QList<Node*> order;
	order << ambient;
	QHash<Node*, int> indexOf;
	indexOf.insert(ambient, 0);
	for (int at = 0; at < order.size(); ++at)
	{
		for (QGraphicsItem* child : order.at(at)->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // a label, not a node
			indexOf.insert(node, int(order.size()));
			order << node;
		}
	}

	pattern.m_nodes.reserve(order.size());
	for (int i = 0; i < order.size(); ++i)
	{
		Node* node = order.at(i);
		PatternNode record = recordOf(node);
		record.parent = i == 0 ? -1 : indexOf.value(dynamic_cast<Node*>(node->parentItem()), -1);
		if (auto* arrow = dynamic_cast<Arrow*>(node))
		{
			record.domain = indexOf.value(arrow->domain(), -1);
			record.codomain = indexOf.value(arrow->codomain(), -1);
		}
		pattern.m_nodes << record;
	}
	if (liveNodes != nullptr)
		*liveNodes = order;

	pattern.index();
	return pattern;
}

Pattern Pattern::fromRulePremise(const Rule& rule)
{
	Pattern pattern;
	if (!rule.isValid() || rule.root() == nullptr)
		return pattern;

	// The order is the contract: root, then the premise objects, then the
	// premise arrows, each in the order the rule itself lists them. A match
	// is stored as positions in these lists and resolved later against a
	// fresh parse of the same file, which gives the same lists again.
	const QList<Node*>& objects = rule.premiseObjects();
	const QList<Arrow*>& arrows = rule.premiseArrows();

	QHash<Node*, int> indexOf;
	indexOf.insert(rule.root(), 0);
	for (int i = 0; i < objects.size(); ++i)
		indexOf.insert(objects.at(i), 1 + i);
	for (int i = 0; i < arrows.size(); ++i)
		indexOf.insert(arrows.at(i), 1 + int(objects.size()) + i);

	PatternNode root = recordOf(rule.root());
	root.parent = -1;
	root.isCategory = true;
	pattern.m_nodes << root;

	for (Node* node : objects)
	{
		PatternNode record = recordOf(node);
		record.parent = indexOf.value(dynamic_cast<Node*>(node->parentItem()), -1);
		pattern.m_nodes << record;
	}
	for (Arrow* arrow : arrows)
	{
		PatternNode record = recordOf(arrow);
		record.parent = indexOf.value(dynamic_cast<Node*>(arrow->parentItem()), -1);
		record.domain = indexOf.value(arrow->domain(), -1);
		record.codomain = indexOf.value(arrow->codomain(), -1);
		pattern.m_nodes << record;
	}

	pattern.m_objectCount = int(objects.size());
	pattern.m_arrowCount = int(arrows.size());
	pattern.index();
	return pattern;
}

// ---------------------------------------------------------------- finding it

namespace
{
	// A straight port of the search that used to walk the live scene, with
	// every pointer turned into an index. The rules it enforces are unchanged:
	// a constant is met only by the same constant, a variable by anything that
	// is not a constant, what the rule claims the diagram must claim too, and
	// the map is injective - two pattern nodes never stand for one diagram
	// node.
	struct Search
	{
		const Pattern& pattern;
		const Pattern& diagram;
		int cap;
		int budget = 200000;   // steps of search, whatever they find
		QList<PatternMatch> found;
		PatternMatch current;
		QSet<int> usedObjects;
		QSet<int> usedArrows;
		QList<int> objectOrder;   // pattern indices, parents first
		QList<int> arrowOrder;

		Search(const Pattern& p, const Pattern& d, int c) : pattern(p), diagram(d), cap(c) {}

		// What the RULE asserts of the piece its pattern sits in, the diagram
		// must assert too. One way only - a diagram may say MORE than the rule
		// asks of it.
		bool claimsAgree(const PatternNode& p, const PatternNode& t) const
		{
			if (p.rowsExact && !t.rowsExact)
				return false;
			if (p.columnsExact && !t.columnsExact)
				return false;
			return true;
		}

		bool labelsAgree(int patternIndex, int targetIndex) const
		{
			const PatternNode& p = pattern.at(patternIndex);
			const PatternNode& t = diagram.at(targetIndex);
			if (Rule::isConstant(p.label))
				return t.label == p.label && claimsAgree(p, t);
			return !Rule::isConstant(t.label) && claimsAgree(p, t);
		}

		bool arrowLabelsAgree(int patternIndex, int targetIndex) const
		{
			const PatternNode& p = pattern.at(patternIndex);
			const PatternNode& t = diagram.at(targetIndex);
			if (Rule::isConstant(p.effectiveLabel))
				return t.effectiveLabel == p.effectiveLabel && claimsAgree(p, t);
			return !Rule::isConstant(t.effectiveLabel) && claimsAgree(p, t);
		}

		void matchArrows(int index)
		{
			if (--budget < 0 || found.size() >= cap)
				return;
			if (index >= arrowOrder.size())
			{
				found << current;
				return;
			}
			const int patternIndex = arrowOrder.at(index);
			const PatternNode& p = pattern.at(patternIndex);
			const int from = current.objects.value(p.domain, -1);
			const int to = current.objects.value(p.codomain, -1);
			const int home = current.objects.value(p.parent, -1);
			if (from < 0 || to < 0 || home < 0)
				return;   // an end outside the pattern: this rule cannot be matched
			for (int candidate : diagram.arrowsIn(home))
			{
				if (usedArrows.contains(candidate))
					continue;
				const PatternNode& t = diagram.at(candidate);
				if (t.domain != from || t.codomain != to)
					continue;
				if (!arrowLabelsAgree(patternIndex, candidate))
					continue;
				usedArrows.insert(candidate);
				current.arrows.insert(patternIndex, candidate);
				matchArrows(index + 1);
				current.arrows.remove(patternIndex);
				usedArrows.remove(candidate);
				if (found.size() >= cap)
					return;
			}
		}

		void matchObjects(int index)
		{
			if (--budget < 0 || found.size() >= cap)
				return;
			if (index >= objectOrder.size())
			{
				matchArrows(0);
				return;
			}
			const int patternIndex = objectOrder.at(index);
			const int home = current.objects.value(pattern.at(patternIndex).parent, -1);
			if (home < 0)
				return;   // its parent was not placed: cannot be
			for (int candidate : diagram.objectsIn(home))
			{
				if (usedObjects.contains(candidate) || candidate == 0)
					continue;   // the ambient category is not an object of itself
				if (!labelsAgree(patternIndex, candidate))
					continue;
				usedObjects.insert(candidate);
				current.objects.insert(patternIndex, candidate);
				matchObjects(index + 1);
				current.objects.remove(patternIndex);
				usedObjects.remove(candidate);
				if (found.size() >= cap)
					return;
			}
		}
	};
}

QList<PatternMatch> PatternMatcher::find(const Pattern& pattern, const Pattern& diagram, int cap)
{
	QList<PatternMatch> none;
	if (pattern.size() == 0 || diagram.size() == 0)
		return none;

	Search search(pattern, diagram, cap);
	for (int i = 1; i < pattern.size(); ++i)
		(pattern.at(i).isArrow ? search.arrowOrder : search.objectOrder) << i;

	// The rule's own ambient category stands for a category of the SAME NAME
	// in the diagram: a rule about R-Mod is about R-Mod, wherever R-Mod is
	// drawn - the canvas itself, or a category drawn inside something.
	for (int root : diagram.rootsNamed(pattern.at(0).label))
	{
		search.current.objects.insert(0, root);
		search.matchObjects(0);
		search.current.objects.remove(0);
		if (search.found.size() >= cap)
			break;
	}
	return search.found;
}
