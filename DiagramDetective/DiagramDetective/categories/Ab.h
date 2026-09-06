#pragma once

#include "../Category.h"

// Abelian groups.
class Ab : public Category
{
	Q_OBJECT
public:
	explicit Ab(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('A'); }
};
