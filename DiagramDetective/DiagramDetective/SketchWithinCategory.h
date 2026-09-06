#pragma once

#include <QMap>
#include <QList>
#include "Object.h"
#include "Category.h"
#include "Arrow.h"

// A sketch: the nodes (objects and arrows) of one category, indexed by id.
// Several nodes may share an id — parallel arrows, or two copies of an object
// in a diagram — so each id keys a list. Membership is tracked, not owned:
// the scene owns the graphics items.
class SketchWithinCategory : public Object
{
	Q_OBJECT

public:
	SketchWithinCategory(const QString& name, Category* category);
	~SketchWithinCategory();

	Category* category() const { return m_category; }

	bool commutes() const { return m_commutes; }
	virtual const QList<Arrow*> equalPaths(Node* domain, Node* codomain) const;

	QList<Node*> nodes(const QString& id) const { return m_nodes.value(id); }
	QList<Object*> objects(const QString& id) const;
	QList<Arrow*> arrows(const QString& id) const;

protected:
	void setCommutes(bool commutes) { m_commutes = commutes; }

	virtual void add(Node* item);
	virtual void remove(Node* item);

private slots:
	// the signals carry their sender, so plain slots suffice
	void onNodeIdChanged(Node* node, const QString& newId);
	void onNodeDeleted(Node* node);

private:
	bool file(Node* node);
	// removes `node` wherever it is filed (its id may have changed since);
	// drops a bucket that becomes empty; true if it was present
	bool unfile(Node* node);
	
	bool m_commutes = true;
	QMap<QString, QList<Node*>> m_nodes;
	Category* m_category = nullptr;
};
