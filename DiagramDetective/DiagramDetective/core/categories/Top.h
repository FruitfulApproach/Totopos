#pragma once

#include "art/Category.h"

// Topological spaces.
class Top : public Category
{
	Q_OBJECT
public:
	explicit Top(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("space"); }
	QString morphismName() const override { return QStringLiteral("continuous map"); }

protected:
	QChar firstLetter() const override { return QChar('X'); }
};
