#pragma once

#include "Arrow.h"
#include "Category.h"

// A functor: an arrow between two categories that live in the same ambient category.
class Functor  : public Arrow
{
	Q_OBJECT

public:
	Functor(const QString& name, Category* dom, Category* cod, QGraphicsItem* parent = nullptr);
	~Functor();
};
