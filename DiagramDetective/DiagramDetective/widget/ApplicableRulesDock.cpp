#include "widget/ApplicableRulesDock.h"

#include <QListWidget>
#include <QLabel>
#include <QPushButton>
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

	m_list = new QListWidget(body);
	m_list->setSelectionMode(QAbstractItemView::SingleSelection);
	m_list->setUniformItemSizes(false);
	m_list->setToolTip("Every rule in the library whose premise is drawn somewhere in this diagram. "
	                   "Pick one to see where it fits; press Apply to draw its conclusion in there.");
	column->addWidget(m_list, 1);

	setWidget(body);

	m_search = new RuleSearch(this);
	connect(m_search, &RuleSearch::updated, this, &ApplicableRulesDock::onUpdated);
	connect(m_search, &RuleSearch::searching, this, &ApplicableRulesDock::onSearching);
	connect(m_list, &QListWidget::itemSelectionChanged, this, &ApplicableRulesDock::onSelectionChanged);
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

QWidget* ApplicableRulesDock::rowFor(const ApplicableRule& rule, int index, bool repeatsName)
{
	auto* row = new QWidget;
	auto* line = new QHBoxLayout(row);
	line->setContentsMargins(4, 3, 4, 3);
	line->setSpacing(8);

	auto* words = new QVBoxLayout;
	words->setContentsMargins(0, 0, 0, 0);
	words->setSpacing(0);

	// The name once per run of same-named rules. Repeating it down a run of
	// eight places the same rule fits would bury the one thing that differs.
	auto* name = new QLabel(repeatsName ? QString() : rule.name.toHtmlEscaped(), row);
	name->setStyleSheet("font-weight: bold;");
	if (repeatsName)
		name->hide();
	words->addWidget(name);

	if (!rule.distinguishing.isEmpty())
	{
		auto* which = new QLabel(Notation::toHtml(rule.distinguishing), row);
		which->setStyleSheet("color: #444;");
		which->setTextFormat(Qt::RichText);
		words->addWidget(which);
	}
	else if (repeatsName)
	{
		// nothing to say and no name to show: keep the row from collapsing
		words->addWidget(new QLabel(rule.name.toHtmlEscaped(), row));
	}
	line->addLayout(words, 1);

	// A rule that neither adds nor takes away is not applied but CITED: it
	// establishes the context rather than changing it.
	auto* apply = new QPushButton(rule.recognises ? "Cite" : "Apply", row);
	apply->setToolTip(rule.recognises
		? QString("Record that %1 holds here, as a step of the proof.").arg(rule.name)
		: QString("Draw what %1 says there is, here.").arg(rule.name));
	connect(apply, &QPushButton::clicked, this, [this, index] { applyAt(index); });
	line->addWidget(apply, 0);

	return row;
}

void ApplicableRulesDock::onUpdated(const QList<ApplicableRule>& rules)
{
	m_rules = rules;
	m_list->clear();

	if (rules.isEmpty())
	{
		m_state->setText("No rule in the library fits this diagram.");
		return;
	}

	int names = 0;
	for (int i = 0; i < rules.size(); ++i)
	{
		const bool repeats = i > 0 && rules.at(i).name == rules.at(i - 1).name;
		if (!repeats)
			++names;
		auto* item = new QListWidgetItem(m_list);
		QWidget* row = rowFor(rules.at(i), i, repeats);
		item->setSizeHint(row->sizeHint());
		m_list->addItem(item);
		m_list->setItemWidget(item, row);
	}

	m_state->setText(QString("%1 rule%2 fit%3 here, in %4 place%5.")
		.arg(names).arg(names == 1 ? "" : "s", names == 1 ? "s" : "")
		.arg(rules.size()).arg(rules.size() == 1 ? "" : "s"));
}

void ApplicableRulesDock::onSelectionChanged()
{
	if (m_scene == nullptr)
		return;
	const int index = m_list->currentRow();
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
