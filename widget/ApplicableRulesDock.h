#pragma once

#include <QDockWidget>
#include <QList>
#include <QSet>

#include "core/rules/RuleSearch.h"

class DiagramScene;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

// Every rule in the library that fits the diagram as it stands, found again
// whenever the diagram changes and listed here.
//
// Rules of the same name sit together, because one rule usually fits in
// several places at once; what tells those places apart is written beside each
// one - only the part that actually differs, so a row says "g: A -> C" and not
// the whole of what it matched. Picking a row lights that place up in the
// diagram; the button on the row draws the rule in there.
class ApplicableRulesDock : public QDockWidget
{
	Q_OBJECT

public:
	explicit ApplicableRulesDock(QWidget* parent = nullptr);

	void setScene(DiagramScene* scene);

	// The pinned rules that FIT the diagram as it stands, one entry each: the
	// first place each one fits. Empty when nothing is pinned, or when what is
	// pinned no longer applies. The window reads this to fill the pills along
	// the foot of the canvas.
	QList<ApplicableRule> pinnedRules() const;
	// draw a pinned rule in, at the first place it fits
	void applyPinned(const QString& name);

signals:
	void message(const QString& text);
	// something was pinned or unpinned, or the list of what fits has changed
	// under the pins: whatever is showing them needs to look again
	void pinnedChanged();

private slots:
	void onUpdated(const QList<ApplicableRule>& rules);
	void onSearching();
	void onSelectionChanged();

private:
	void applyAt(int index);
	// the row under a heading: what tells this place from the others, and the
	// button that draws the rule in there
	QWidget* rowFor(const ApplicableRule& rule, int index);

	DiagramScene* m_scene = nullptr;
	RuleSearch* m_search = nullptr;
	QTreeWidget* m_tree = nullptr;
	QLabel* m_state = nullptr;
	QList<ApplicableRule> m_rules;
	// Which headings the user has folded away. The list is rebuilt from
	// scratch on every change to the diagram, so without this every search
	// would spring them all open again.
	QSet<QString> m_collapsed;
	// Rules kept to hand, BY NAME. Not by place: the list is rebuilt from
	// scratch whenever the diagram changes, so an index would come back
	// meaning a different place (see rowFor).
	QSet<QString> m_pinned;
};
