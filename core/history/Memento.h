#pragma once

#include <QString>
#include <QByteArray>

// what a memento is, in a file
namespace MementoTag
{
	enum Tag : quint16
	{
		Unknown      = 0,
		ItemsMoved   = 1,
		StyleChanged = 2,
		NodesCreated = 3,
		NodesRemoved = 4,
		ExistsSuch   = 5,
		ArrowBent    = 6,
		Renamed      = 7,
		LabelMoved   = 8,
		Exactness    = 9,
		Statement    = 10,
		Note         = 11,
		DeleteMark   = 12,
		RuleApplied  = 13,
		ArrowStyle   = 14,
		// from the other line of work: an arrow's claims as props, and a
		// node rebuilt as another kind. Numbered after the above rather
		// than over it - a tag is what a saved step is read back BY.
		Monic        = 15,
		Epic         = 16,
		NodeRetyped  = 17,
		// 18, 19 and 20 were the proposition BOX and the pieces it could be
		// read in; 22 was a tag put on a node. Both were attempts at holding
		// several statements in one file, and a file says ONE thing (see
		// Node). The numbers are not reused: a tag is what a saved step is
		// read back BY, and an old file still carries steps that say 18.
		PropositionMade = 18,
		Piecewise       = 19,
		PieceRenamed    = 20,
		// the claim that what is drawn under a node commutes
		Commutes        = 21,
		// 22: a statement tag put on a node, which no longer exists
	};
}

// One change to the scene, holding what it takes to put the scene back the
// way it was - and to do it again. The history keeps these in order, so the
// scene can be read as the sequence of things that were done to it.
class Memento
{
public:
	explicit Memento(const QString& description) : m_description(description) {}
	virtual ~Memento() {}

	QString describe() const { return m_description; }
	void setDescription(const QString& description) { m_description = description; }

	// the scene as it was before this change / as it was after it
	virtual void undo() = 0;
	virtual void redo() = 0;

	// A change that only touches how the diagram LOOKS - where things sit,
	// what colour they are. Nothing mathematical happened. PureGraphical says
	// so, so the history can be read as a proof with these left out.
	virtual bool isPureGraphical() const { return false; }

	// What this change is, written for the file: a tag saying which kind it
	// was and a payload describing it without pointers. Only changes that are
	// NOT purely graphical are written - where a node was dragged to is part
	// of the diagram itself, not of its history.
	virtual quint16 typeTag() const { return MementoTag::Unknown; }
	virtual QByteArray payload() const { return QByteArray(); }

	// A change read back from a file tells what was done but no longer holds
	// the items it did it to, so it cannot be walked back.
	virtual bool canRestore() const { return true; }

private:
	QString m_description;
};

// The base of every change that says nothing about the mathematics.
class PureGraphical : public Memento
{
public:
	explicit PureGraphical(const QString& description) : Memento(description) {}
	bool isPureGraphical() const override { return true; }
};
