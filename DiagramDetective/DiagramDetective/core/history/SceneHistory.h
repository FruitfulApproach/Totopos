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
	int m_suspended = 0;
};
