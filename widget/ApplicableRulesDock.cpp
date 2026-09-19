#include "widget/ApplicableRulesDock.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontMetrics>

#include "art/DiagramScene.h"
#include "core/Emoji.h"
#include "core/Notation.h"

ApplicableRulesDock::ApplicableRulesDock(QWidget* parent)
	: QDockWidget(Emoji::library() + "  Applicable Rules", parent)
{
	setObjectName("applicableRulesDock");

	auto* body = new QWidget(this);
	auto* column = new QVBoxLayout(body);
	column->setContentsMargins(6, 6, 6, 6);
	column->setSpacing(4);

	m_state = new QLabel("Nothing yet.", body);
	m_state->setWordWrap(true);
	m_state->setStyleSheet("color: #555;");
	column->addWidget(m_state);

	// A rule usually fits in several places at once, so the places live under
	// the rule's name and can be folded away: a library of seventeen rules in
	// fifty-six places is a wall of rows otherwise.
	m_tree = new QTreeWidget(body);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setIndentation(14);
	m_tree->setUniformRowHeights(false);
	m_tree->setToolTip("Every rule in the library whose premise is drawn somewhere in this diagram. "
	                   "Pick a place to see where it fits; press Apply to draw the rule in there.");
	column->addWidget(m_tree, 1);

	setWidget(body);

	m_search = new RuleSearch(this);
	connect(m_search, &RuleSearch::updated, this, &ApplicableRulesDock::onUpdated);
	connect(m_search, &RuleSearch::searching, this, &ApplicableRulesDock::onSearching);
	connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ApplicableRulesDock::onSelectionChanged);
	// remember what has been folded away, by name: the rows themselves do not
	// survive the next search
	connect(m_tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item) {
		if (item->parent() == nullptr)
			m_collapsed.insert(item->text(0));
	});
	connect(m_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
		if (item->parent() == nullptr)
			m_collapsed.remove(item->text(0));
	});
}

void ApplicableRulesDock::setScene(DiagramScene* scene)
{
	m_scene = scene;
	m_search->setScene(scene);
}

void ApplicableRulesDock::onSearching()
{
	m_state->setText("Looking through the library...");
}

QWidget* ApplicableRulesDock::rowFor(const ApplicableRule& rule, int index)
{
	auto* row = new QWidget;
	auto* line = new QHBoxLayout(row);
	line->setContentsMargins(4, 2, 4, 2);
	line->setSpacing(8);

	// Only what tells this place from the others of the same rule - the name
	// is the heading above it. A rule that fits in one place alone has nothing
	// to be told apart from, and says so.
	auto* which = new QLabel(rule.distinguishing.isEmpty()
		? QStringLiteral("<i>the one place it fits</i>")
		: Notation::toHtml(rule.distinguishing), row);
	which->setStyleSheet("color: #444;");
	which->setTextFormat(Qt::RichText);
	line->addWidget(which, 1);

	// A rule that neither adds nor takes away is not applied but CITED: it
	// establishes the context rather than changing it.
	auto* apply = new QPushButton(rule.recognises ? "Cite" : "Apply", row);
	apply->setToolTip(rule.recognises
		? QString("Record that %1 holds here, as a step of the proof.").arg(rule.name)
		: QString("Draw what %1 says there is, here.").arg(rule.name));
	connect(apply, &QPushButton::clicked, this, [this, index] { applyAt(index); });
	line->addWidget(apply, 0);

	// PINNED: keep this one where the work is.
	//
	// A rule you are using repeatedly should not need a trip to this dock
	// every time. Pin it and it also appears as a pill along the foot of the
	// canvas, for as long as it goes on fitting - unpin it, or change the
	// diagram so it no longer fits, and the pill goes.
	//
	// Pinned by NAME, not by place. A rule fits wherever it fits, and which
	// of those places is the third one in a list is not something anybody
	// means to remember: the list is rebuilt from scratch every time the
	// diagram changes, so an index would pin a different place by morning.
	auto* pin = new QToolButton(row);
	pin->setCheckable(true);
	pin->setChecked(m_pinned.contains(rule.name));
	pin->setText(QStringLiteral(u"\U0001F4CC"));   // a pushpin - above the BMP, so not a QChar
	pin->setAutoRaise(true);
	pin->setToolTip(QString("Keep %1 to hand: pinned, it also sits along the foot of the canvas "
	                        "whenever it fits the diagram.").arg(rule.name));
	const QString name = rule.name;
	connect(pin, &QToolButton::toggled, this, [this, name](bool on) {
		if (on)
			m_pinned.insert(name);
		else
			m_pinned.remove(name);
		emit pinnedChanged();
	});
	line->addWidget(pin, 0);

	return row;
}

QList<ApplicableRule> ApplicableRulesDock::pinnedRules() const
{
	// The first place each pinned rule fits, and only the first: a pill is
	// one button, and "apply this rule" with no place named can only sensibly
	// mean the first place it fits. The dock is where you choose BETWEEN
	// places, and it is still there for that.
	QList<ApplicableRule> found;
	QSet<QString> seen;
	for (const ApplicableRule& rule : m_rules)
		if (m_pinned.contains(rule.name) && !seen.contains(rule.name))
		{
			seen.insert(rule.name);
			found << rule;
		}
	return found;
}

void ApplicableRulesDock::applyPinned(const QString& name)
{
	for (int i = 0; i < m_rules.size(); ++i)
		if (m_rules.at(i).name == name)
		{
			applyAt(i);
			return;
		}
	emit message(QString("%1 no longer fits this diagram.").arg(name));
}

void ApplicableRulesDock::onUpdated(const QList<ApplicableRule>& rules)
{
	m_rules = rules;
	m_tree->clear();

	if (rules.isEmpty())
	{
		m_state->setText("No rule in the library fits this diagram.");
		emit pinnedChanged();   // nothing fits, so nothing pinned fits either
		return;
	}

	// The search hands them over with same-named rules already together, so
	// one pass over the list is one pass over the headings.
	int names = 0;
	for (int from = 0; from < rules.size(); )
	{
		int to = from + 1;
		while (to < rules.size() && rules.at(to).name == rules.at(from).name)
			++to;
		++names;

		const QString name = rules.at(from).name;
		const int places = to - from;
		auto* heading = new QTreeWidgetItem(m_tree);
		heading->setText(0, places > 1 ? QString("%1  -  %2 places").arg(name).arg(places) : name);
		QFont bold = heading->font(0);
		bold.setBold(true);
		heading->setFont(0, bold);
		heading->setToolTip(0, rules.at(from).path);
		heading->setData(0, Qt::UserRole, -1);

		for (int i = from; i < to; ++i)
		{
			auto* place = new QTreeWidgetItem(heading);
			place->setData(0, Qt::UserRole, i);
			QWidget* row = rowFor(rules.at(i), i);
			place->setSizeHint(0, row->sizeHint());
			m_tree->setItemWidget(place, 0, row);
		}
		// folded away last time round, so folded away now
		heading->setExpanded(!m_collapsed.contains(heading->text(0)));
		from = to;
	}

	m_state->setText(QString("%1 rule%2 fit%3 here, in %4 place%5.")
		.arg(names).arg(names == 1 ? "" : "s", names == 1 ? "s" : "")
		.arg(rules.size()).arg(rules.size() == 1 ? "" : "s"));

	// What fits has just changed, so what is pinned AND fits may have changed
	// with it - even though nobody touched a pin.
	emit pinnedChanged();
}

void ApplicableRulesDock::onSelectionChanged()
{
	if (m_scene == nullptr)
		return;
	QTreeWidgetItem* item = m_tree->currentItem();
	const int index = item != nullptr ? item->data(0, Qt::UserRole).toInt() : -1;
	if (index < 0 || index >= m_rules.size())
	{
		m_scene->clearMatch();
		return;
	}
	// light up exactly what this rule matched, and step the rest back
	m_scene->showMatch(m_search->nodesOf(m_rules.at(index)));
}

void ApplicableRulesDock::applyAt(int index)
{
	if (index < 0 || index >= m_rules.size() || m_scene == nullptr)
		return;
	const ApplicableRule rule = m_rules.at(index);

	// the lighting belongs to a diagram that is about to change
	m_scene->clearMatch();

	if (!m_search->apply(rule))
	{
		emit message(QString("%1 no longer fits there - the diagram has changed since it was found.")
			.arg(rule.name));
		return;
	}
	emit message(rule.recognises
		? QString("Cited %1.").arg(rule.name)
		: QString("Applied %1%2.").arg(rule.name,
			rule.distinguishing.isEmpty() ? QString() : QString(" at %1").arg(rule.distinguishing)));
}
