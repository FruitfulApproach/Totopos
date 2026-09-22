#include "core/rules/RuleSearch.h"

#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

#include "core/rules/Rule.h"
#include "core/rules/Library.h"
#include "art/DiagramScene.h"
#include "art/Category.h"
#include "art/Arrow.h"
#include "core/history/SceneHistory.h"

namespace
{
	const QChar kTo(0x2192);   // ->
	const int kMaxFiles = 600;

	QString labelOf(const Pattern& diagram, int index)
	{
		if (index < 0 || index >= diagram.size())
			return QStringLiteral("?");
		const QString label = diagram.at(index).label;
		return label.isEmpty() ? QStringLiteral("an unnamed one") : label;
	}

	// "g: A -> C" for an arrow, its own name and its two ends, all named as
	// the DIAGRAM names them. What tells one match of a rule from another is
	// what it matched, never what the rule calls it.
	QString describeArrow(const Pattern& diagram, int index)
	{
		if (index < 0 || index >= diagram.size())
			return QString();
		const PatternNode& node = diagram.at(index);
		const QString name = node.effectiveLabel.isEmpty() ? node.label : node.effectiveLabel;
		return QString("%1: %2 %3 %4")
			.arg(name.isEmpty() ? QStringLiteral("an unnamed arrow") : name,
			     labelOf(diagram, node.domain), QString(kTo), labelOf(diagram, node.codomain));
	}

	// Of everything this group of matches stands on, keep only the parts that
	// actually differ between them - the rest is the same wherever the rule
	// fits, so saying it would tell the reader nothing. A rule that fits in
	// one place alone has nothing to be told apart from, and gets no text.
	void tellApart(QList<ApplicableRule>& rules, int from, int to,
	               const QList<QStringList>& arrowTexts, const QList<QStringList>& objectTexts)
	{
		const int count = to - from;
		if (count < 2)
			return;

		const int arrowSlots = arrowTexts.at(from).size();
		const int objectSlots = objectTexts.at(from).size();

		QList<bool> arrowVaries(arrowSlots, false);
		QList<bool> objectVaries(objectSlots, false);
		for (int slot = 0; slot < arrowSlots; ++slot)
			for (int i = from + 1; i < to; ++i)
				if (arrowTexts.at(i).value(slot) != arrowTexts.at(from).value(slot))
				{
					arrowVaries[slot] = true;
					break;
				}
		for (int slot = 0; slot < objectSlots; ++slot)
			for (int i = from + 1; i < to; ++i)
				if (objectTexts.at(i).value(slot) != objectTexts.at(from).value(slot))
				{
					objectVaries[slot] = true;
					break;
				}

		for (int i = from; i < to; ++i)
		{
			QStringList parts;
			// arrows first: an arrow carries its ends with it, so it usually
			// says the whole of what is different on its own
			for (int slot = 0; slot < arrowSlots; ++slot)
				if (arrowVaries.at(slot))
					parts << arrowTexts.at(i).value(slot);
			if (parts.isEmpty())
				for (int slot = 0; slot < objectSlots; ++slot)
					if (objectVaries.at(slot))
						parts << objectTexts.at(i).value(slot);
			rules[i].distinguishing = parts.join(QStringLiteral(", "));
		}
	}
}

// ---------------------------------------------------------------- the far side

void RuleSearchWorker::search(const Pattern& diagram, const QString& libraryRoot, quint64 generation)
{
	QList<ApplicableRule> rules;
	QList<QStringList> arrowTexts, objectTexts;

	if (diagram.size() > 0 && !libraryRoot.isEmpty())
	{
		QDirIterator files(libraryRoot, { QStringLiteral("*.totopos") },
		                   QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
		int seen = 0;
		while (files.hasNext() && seen < kMaxFiles)
		{
			const QString path = files.next();
			++seen;

			// The rule and the scene it is read into belong to this thread
			// alone: made here, used here, thrown away here. Nothing on the
			// GUI side ever sees them.
			Rule rule(path);
			if (!rule.isValid() || rule.root() == nullptr)
				continue;
			const Pattern premise = Pattern::fromRulePremise(rule);
			if (premise.size() == 0)
				continue;

			for (const PatternMatch& match : PatternMatcher::find(premise, diagram))
			{
				ApplicableRule found;
				found.name = rule.name();
				found.path = path;
				found.recognises = rule.isRecogniser();
				// Pattern::Universe is a root as good as any: it says the
				// rule was read one universe up, with its C standing for the
				// canvas. Nothing is drawn there, so there is no node to
				// resolve it to - and none is needed (see RuleSearch::apply).
				found.root = match.objects.value(0, -1);
				if (found.root == -1)
					continue;

				QStringList arrowsSaid, objectsSaid;
				bool whole = true;
				for (int i = 0; i < premise.objectCount(); ++i)
				{
					const int at = match.objects.value(1 + i, -1);
					if (at < 0) { whole = false; break; }
					found.objects << at;
					objectsSaid << labelOf(diagram, at);
					const QString variable = premise.at(1 + i).label;
					if (!variable.isEmpty())
						found.bindings.insert(variable, diagram.at(at).label);
				}
				for (int i = 0; whole && i < premise.arrowCount(); ++i)
				{
					const int at = match.arrows.value(1 + premise.objectCount() + i, -1);
					if (at < 0) { whole = false; break; }
					found.arrows << at;
					arrowsSaid << describeArrow(diagram, at);
					const QString variable = premise.at(1 + premise.objectCount() + i).label;
					if (!variable.isEmpty())
						found.bindings.insert(variable, diagram.at(at).label);
				}
				if (!whole)
					continue;

				rules << found;
				arrowTexts << arrowsSaid;
				objectTexts << objectsSaid;
			}
		}
	}

	// Same name together, and in a settled order so the list does not shuffle
	// itself between one search and the next.
	QList<int> order;
	for (int i = 0; i < rules.size(); ++i)
		order << i;
	std::stable_sort(order.begin(), order.end(), [&rules](int a, int b) {
		const int byName = QString::compare(rules.at(a).name, rules.at(b).name, Qt::CaseInsensitive);
		if (byName != 0)
			return byName < 0;
		return rules.at(a).path < rules.at(b).path;
	});
	QList<ApplicableRule> sorted;
	QList<QStringList> sortedArrows, sortedObjects;
	for (int i : order)
	{
		sorted << rules.at(i);
		sortedArrows << arrowTexts.at(i);
		sortedObjects << objectTexts.at(i);
	}

	// then, run of same-named rules by run of same-named rules, work out what
	// separates them
	for (int from = 0; from < sorted.size(); )
	{
		int to = from + 1;
		while (to < sorted.size() && sorted.at(to).name == sorted.at(from).name)
			++to;
		tellApart(sorted, from, to, sortedArrows, sortedObjects);
		from = to;
	}

	emit found(sorted, generation);
}

// ---------------------------------------------------------------- this side

RuleSearch::RuleSearch(QObject* parent)
	: QObject(parent)
{
	qRegisterMetaType<Pattern>("Pattern");
	qRegisterMetaType<ApplicableRule>("ApplicableRule");
	qRegisterMetaType<QList<ApplicableRule>>("QList<ApplicableRule>");

	m_worker = new RuleSearchWorker;
	m_worker->moveToThread(&m_thread);
	connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
	connect(m_worker, &RuleSearchWorker::found, this, &RuleSearch::onFound);
	m_thread.start();

	// A burst of edits is one change as far as this is concerned: the search
	// waits for the typing to stop rather than starting over on each keystroke.
	m_debounce.setSingleShot(true);
	m_debounce.setInterval(350);
	connect(&m_debounce, &QTimer::timeout, this, &RuleSearch::start);
}

RuleSearch::~RuleSearch()
{
	m_thread.quit();
	m_thread.wait();
}

void RuleSearch::setScene(DiagramScene* scene)
{
	if (m_scene == scene)
		return;
	if (!m_scene.isNull() && m_scene->history() != nullptr)
		disconnect(m_scene->history(), nullptr, this, nullptr);
	m_scene = scene;
	if (scene != nullptr && scene->history() != nullptr)
		connect(scene->history(), &SceneHistory::changed, this, &RuleSearch::scheduleSearch);
	scheduleSearch();
}

void RuleSearch::scheduleSearch()
{
	// Whatever the last answer was about, it is about a diagram that no longer
	// exists. Let go of the nodes at once - an answer still in flight will be
	// dropped when it lands, and nothing may be applied in the meantime.
	++m_generation;
	m_liveNodes.clear();
	m_debounce.start();
}

void RuleSearch::start()
{
	if (m_scene.isNull() || m_scene->ambientCategory() == nullptr)
	{
		m_rules.clear();
		emit updated(m_rules);
		return;
	}

	// The one moment the live diagram is read. Everything after this runs on
	// the copy.
	QList<Node*> live;
	const Pattern snapshot = Pattern::fromDiagram(m_scene->ambientCategory(), &live);
	m_liveNodes.clear();
	m_liveNodes.reserve(live.size());
	for (Node* node : live)
		m_liveNodes << QPointer<Node>(node);
	m_answered = m_generation;

	emit searching();
	QMetaObject::invokeMethod(m_worker, "search", Qt::QueuedConnection,
	                          Q_ARG(Pattern, snapshot),
	                          Q_ARG(QString, Library::root()),
	                          Q_ARG(quint64, m_generation));
}

void RuleSearch::onFound(const QList<ApplicableRule>& rules, quint64 generation)
{
	if (generation != m_generation || generation != m_answered)
		return;   // the diagram moved on while we were looking: this is about a past one
	m_rules = rules;
	emit updated(m_rules);
}

Node* RuleSearch::nodeAt(int index) const
{
	return index >= 0 && index < m_liveNodes.size() ? m_liveNodes.at(index).data() : nullptr;
}

QList<Node*> RuleSearch::nodesOf(const ApplicableRule& rule) const
{
	QList<Node*> nodes;
	for (int at : rule.objects)
		if (Node* node = nodeAt(at))
			nodes << node;
	for (int at : rule.arrows)
		if (Node* node = nodeAt(at))
			nodes << node;
	return nodes;
}

bool RuleSearch::apply(const ApplicableRule& found)
{
	if (m_scene.isNull() || m_liveNodes.isEmpty())
		return false;

	// Read the rule again rather than keeping every one of them loaded. The
	// premise lists come back in the same order they were in when the match
	// was made, which is what the positions in `found` are counted against.
	Rule rule(found.path);
	if (!rule.isValid() || rule.root() == nullptr)
		return false;
	if (rule.premiseObjects().size() != found.objects.size()
	 || rule.premiseArrows().size() != found.arrows.size())
		return false;   // the file changed under us

	// The root binds to nothing when the rule was read one universe up: the
	// universe is not drawn, and a rule read that way puts nothing in it, so
	// nothing ever asks what it stands for.
	RuleMatch match;
	if (found.root != Pattern::Universe)
	{
		Node* root = nodeAt(found.root);
		if (root == nullptr)
			return false;
		match.objects.insert(rule.root(), root);
	}
	for (int i = 0; i < found.objects.size(); ++i)
	{
		Node* node = nodeAt(found.objects.at(i));
		if (node == nullptr)
			return false;
		match.objects.insert(rule.premiseObjects().at(i), node);
	}
	for (int i = 0; i < found.arrows.size(); ++i)
	{
		auto* arrow = dynamic_cast<Arrow*>(nodeAt(found.arrows.at(i)));
		if (arrow == nullptr)
			return false;
		match.arrows.insert(rule.premiseArrows().at(i), arrow);
	}

	RuleMatcher::apply(rule, match, m_scene.data());
	return true;
}
