#pragma once

#include "../Category.h"

// Right R-modules.
class ModR : public Category
{
	Q_OBJECT
public:
	explicit ModR(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('M'); }
};
