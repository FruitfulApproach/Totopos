#pragma once

#include "../Category.h"

// Topological spaces.
class Top : public Category
{
	Q_OBJECT
public:
	explicit Top(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('X'); }
};
