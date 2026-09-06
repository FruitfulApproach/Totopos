#pragma once

#include "Node.h"

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

signals:
	void domainChanged(Node* domain);
	void codomainChanged(Node* codomain);

private slots:
	void onObjectDeleted(Node*) { deleteLater(); }
	void onObjectMoved(Node*, const QPointF&) { update(); }

protected:
	void connectToObject(Node* object);
	void disconnectFromObject(Node* object);

private:
	Node* m_domain = nullptr;
	Node* m_codomain = nullptr;
};

