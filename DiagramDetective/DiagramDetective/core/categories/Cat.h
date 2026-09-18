#pragma once

#include "art/Category.h"
#include "art/Functor.h"

// Small categories: an object of Cat is a (locally small) category.
class Cat : public Category
{
	Q_OBJECT
public:
	explicit Cat(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("category"); }
	QString morphismName() const override { return QStringLiteral("functor"); }

protected:
	Object* makeObject(const QString& name) override;
	QChar firstLetter() const override { return QChar('C'); }
	// an arrow between categories is a functor
	Arrow* createArrow(const QString& name, Node* from, Node* to) override;
	QChar firstArrowLetter() const override { return QChar('F'); }
};
