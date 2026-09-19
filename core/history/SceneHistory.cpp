#include "core/history/SceneHistory.h"
#include "core/history/Memento.h"

SceneHistory::SceneHistory(QObject* parent)
	: QObject(parent)
{
}

SceneHistory::~SceneHistory()
{
	qDeleteAll(m_mementos);
}

void SceneHistory::suspend(bool on)
{
	m_suspended += on ? 1 : -1;
	if (m_suspended < 0)
		m_suspended = 0;
}

void SceneHistory::dropUndone()
{
	// anything undone is now a road not taken: the mementos that held the
	// items taken out of the scene destroy them as they go
	// THE SAVED POINT MAY BE ON THE ROAD NOT TAKEN.
	//
	// Undo back past where the file was written, then do something new: the
	// steps between are thrown away here, and the position they were counted
	// from no longer names anything. The diagram cannot get back to what is on
	// disk by any amount of undoing, so it is modified and must stay modified.
	// -1 is a position nothing can reach.
	if (m_savedAt > m_position)
		m_savedAt = -1;
	while (m_mementos.size() > m_position)
		delete m_mementos.takeLast();
}

void SceneHistory::record(Memento* memento)
{
	if (memento == nullptr)
		return;
	if (isSuspended())
	{
		delete memento;   // replaying: this is not new history
		return;
	}
	dropUndone();
	m_mementos.append(memento);
	m_position = m_mementos.size();
	emit changed();
}

QString SceneHistory::undoText() const
{
	return canUndo() ? m_mementos.at(m_position - 1)->describe() : QString();
}

QString SceneHistory::redoText() const
{
	return canRedo() ? m_mementos.at(m_position)->describe() : QString();
}

void SceneHistory::undo()
{
	if (!canUndo())
		return;
	suspend(true);
	m_mementos.at(--m_position)->undo();
	suspend(false);
	emit changed();
}

void SceneHistory::redo()
{
	if (!canRedo())
		return;
	suspend(true);
	m_mementos.at(m_position++)->redo();
	suspend(false);
	emit changed();
}

void SceneHistory::clear()
{
	qDeleteAll(m_mementos);
	m_mementos.clear();
	m_position = 0;
	m_savedAt = 0;   // an empty diagram is a saved one: there is nothing in it to lose
	emit changed();
}

void SceneHistory::markSaved()
{
	if (m_savedAt == m_position)
		return;
	m_savedAt = m_position;
	emit changed();   // the asterisk on the tab and in the title goes
}

QList<Memento*> SceneHistory::structural() const
{
	QList<Memento*> steps;
	for (Memento* m : m_mementos)
		if (!m->isPureGraphical())
			steps << m;
	return steps;
}

QStringList SceneHistory::structuralDescriptions() const
{
	QStringList lines;
	for (Memento* m : structural())
		lines << m->describe();
	return lines;
}
