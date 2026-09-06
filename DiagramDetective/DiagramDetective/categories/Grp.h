#pragma once

#include "../Category.h"

// Groups.
class Grp : public Category
{
	Q_OBJECT
public:
	explicit Grp(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('G'); }
};
