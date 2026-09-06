#include "Arrow.h"

Arrow::Arrow(const QString& id, Node* domain, Node* codomain, QGraphicsItem *parent)
	: Node(id, parent)
{
	setDomain(domain);
	setCodomain(codomain);
}

void Arrow::setDomain(Node* domain)
{
	if (domain != m_domain)
	{
		if (m_domain != nullptr)
		{
			disconnectFromObject(m_domain);
		}

		m_domain = domain;
		connectToObject(domain);
		emit domainChanged(domain);
	}
}

void Arrow::setCodomain(Node* codomain)
{
	if (codomain != m_codomain)
	{
		if (m_codomain != nullptr)
		{
			disconnectFromObject(m_codomain);
		}

		m_codomain = codomain;
		connectToObject(codomain);
		emit codomainChanged(codomain);
	}
}

Arrow::~Arrow()
{
	if (m_domain != nullptr)
	{
		disconnectFromObject(m_domain);
	}
	if (m_codomain != nullptr)
	{
		disconnectFromObject(m_codomain);
	}
}

void Arrow::connectToObject(Node * object)
{
	connect(object, &Node::deleted, this, &Arrow::onObjectDeleted);
	connect(object, &Node::moved, this, &Arrow::onObjectMoved);
}

void Arrow::disconnectFromObject(Node* object)
{
	disconnect(object, &Node::deleted, this, &Arrow::onObjectDeleted);
	disconnect(object, &Node::moved, this, &Arrow::onObjectMoved);
}
