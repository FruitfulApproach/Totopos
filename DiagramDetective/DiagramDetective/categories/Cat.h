#pragma once

#include "../Category.h"

// Small categories: an object of Cat is a (locally small) category.
class Cat : public Category
{
	Q_OBJECT
public:
	explicit Cat(QGraphicsItem* parent = nullptr);

protected:
	Object* makeObject(const QString& name) override;
	QChar firstLetter() const override { return QChar('C'); }
};
