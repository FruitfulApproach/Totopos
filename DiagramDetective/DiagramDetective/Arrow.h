#pragma once

#include "Node.h"

// An arrow between two nodes, drawn from the edge of its domain to the edge
// of its codomain and following them as they move. Its label rides the
// midpoint. Its own pos() is irrelevant; the ends decide its geometry.
class Arrow  : public Node
{
	Q_OBJECT

public:
	Arrow(const QString& id, Node* domain, Node* codomain, QGraphicsItem *parent=nullptr);

	Node* domain() const { return m_domain; }
	Node* codomain() const { return m_codomain; }
	virtual void setDomain(Node* domain);
	virtual void setCodomain(Node* codomain);

	~Arrow();

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
	void domainChanged(Node* domain);
	void codomainChanged(Node* codomain);

private slots:
	void onObjectDeleted(Node*) { deleteLater(); }
	void onObjectMoved(Node*, const QPointF&) { refreshGeometry(); }

protected:
	void connectToObject(Node* object);
	void disconnectFromObject(Node* object);

	// the segment in our coordinates, from the domain's frame to the codomain's
	bool segment(QPointF& from, QPointF& to) const;
	// the ends moved or changed: re-index, re-place the label, repaint
	void refreshGeometry();

private:
	Node* m_domain = nullptr;
	Node* m_codomain = nullptr;
};
