#pragma once

#include "../Category.h"

// Right R-modules.
class ModR : public Category
{
	Q_OBJECT
public:
	explicit ModR(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("R-module"); }
	QString morphismName() const override { return QStringLiteral("R-linear map"); }

protected:
	QChar firstLetter() const override { return QChar('M'); }
};
