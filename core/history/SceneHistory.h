#pragma once

#include <QObject>
#include <QList>
#include <QStringList>

class Memento;

// The scene's history: one memento per change, in the order they happened,
// with a position saying how many of them are currently applied. Undo walks
// back through them, redo forward; recording a new change throws away
// anything that had been undone.
class SceneHistory : public QObject
{
	Q_OBJECT

public:
	explicit SceneHistory(QObject* parent = nullptr);
	~SceneHistory() override;

	// takes ownership; the change must ALREADY have been made
	void record(Memento* memento);

	bool canUndo() const { return m_position > 0; }
	bool canRedo() const { return m_position < m_mementos.size(); }
	QString undoText() const;
	QString redoText() const;

	// every change, oldest first, and how many of them are in force
	const QList<Memento*>& mementos() const { return m_mementos; }
	int position() const { return m_position; }

	// The changes that say something about the mathematics: the purely
	// graphical ones (a node dragged, a colour picked) are left out. This is
	// the history read as proof steps.
	QList<Memento*> structural() const;
	QStringList structuralDescriptions() const;

	// HAS THIS DIAGRAM CHANGED SINCE IT WAS LAST SAVED?
	//
	// Kept as the POSITION the file was written at rather than as a flag,
	// because a flag cannot be undone. Draw something, save, draw another
	// thing, undo it: a flag would still say modified, when in fact what is
	// on screen is exactly what is on disk again. Comparing positions gets
	// that right, and gets redoing back out of it right too.
	void markSaved();
	bool isModified() const { return m_position != m_savedAt; }

	// While suspended nothing is recorded: undo and redo change the scene, and
	// those changes are not new history.
	void suspend(bool on);
	bool isSuspended() const { return m_suspended > 0; }

public slots:
	void undo();
	void redo();
	void clear();

signals:
	void changed();

private:
	void dropUndone();

	QList<Memento*> m_mementos;
	int m_position = 0;
	int m_savedAt = 0;      // where in the history the file on disk stands
	int m_suspended = 0;
};
