#pragma once

#include <QObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QMetaType>

#include "core/rules/Pattern.h"

class DiagramScene;
class Node;

// One place one rule of the library fits the diagram as it stands.
//
// It carries no pointers. The diagram side is held as indices into the Pattern
// the search ran against, and the rule side as POSITIONS in the rule's own
// premise lists - so the whole thing can be sent between threads, and the rule
// itself need not be kept loaded. Applying it parses the file again and lines
// the positions up (Rule::premiseObjects() comes back in the same order).
struct ApplicableRule
{
	QString name;            // what the rule calls itself
	QString path;            // the file it came from
	QString distinguishing;  // what tells this match from the others of the same rule
	bool recognises = false; // a rule that only says the scene is there: cited, not applied

	int root = -1;                   // diagram index of the category it sits in
	QList<int> objects;              // premise object position -> diagram index
	QList<int> arrows;               // premise arrow position -> diagram index

	// what the rule's variables stand for here, for the history and for
	// working out which parts are worth showing
	QHash<QString, QString> bindings;
};
Q_DECLARE_METATYPE(ApplicableRule)
Q_DECLARE_METATYPE(QList<ApplicableRule>)

// The half that runs in the other thread. It is handed a snapshot and a
// folder, and never sees a live node.
class RuleSearchWorker : public QObject
{
	Q_OBJECT

public slots:
	// Walk the library, read every rule, and collect every place each one
	// fits. `generation` comes back untouched so a stale answer can be thrown
	// away.
	void search(const Pattern& diagram, const QString& libraryRoot, quint64 generation);

signals:
	void found(const QList<ApplicableRule>& rules, quint64 generation);
};

// The half that lives here. It snapshots the diagram, starts the thread, drops
// answers that arrived too late, and hands what survives to whoever is
// listening.
class RuleSearch : public QObject
{
	Q_OBJECT

public:
	explicit RuleSearch(QObject* parent = nullptr);
	~RuleSearch() override;

	void setScene(DiagramScene* scene);

	// Search again shortly. Called on every change to the diagram, so it is
	// debounced: a burst of edits makes one search, once the burst stops.
	void scheduleSearch();

	// The live nodes the last answer's indices point at. Empty once the
	// diagram has changed under it.
	Node* nodeAt(int index) const;
	// every node and arrow one match stands on, for lighting it up
	QList<Node*> nodesOf(const ApplicableRule& rule) const;

	// Draw this one in. Parses the rule again, lines its premise up with what
	// the match found, and applies it - all on this thread, as one step of the
	// history. False when the diagram has moved on and the match no longer
	// stands.
	bool apply(const ApplicableRule& rule);

signals:
	// a fresh answer, in the order they should be listed
	void updated(const QList<ApplicableRule>& rules);
	void searching();

private slots:
	void onFound(const QList<ApplicableRule>& rules, quint64 generation);

private:
	void start();

	QPointer<DiagramScene> m_scene;
	QThread m_thread;
	RuleSearchWorker* m_worker = nullptr;
	QTimer m_debounce;

	quint64 m_generation = 0;   // bumped whenever the diagram changes
	quint64 m_answered = 0;     // the generation the held nodes belong to
	// index -> node, for the generation just answered. QPointer because a node
	// can go between the answer landing and a row of the panel being pressed.
	QList<QPointer<Node>> m_liveNodes;
	QList<ApplicableRule> m_rules;
};
