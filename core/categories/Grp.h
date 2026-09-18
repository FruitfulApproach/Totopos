#pragma once

#include "art/Category.h"

// Groups.
class Grp : public Category
{
	Q_OBJECT
public:
	explicit Grp(QGraphicsItem* parent = nullptr);

	QString builtInName() const override { return QStringLiteral("Grp"); }

public:
	QString objectName() const override { return QStringLiteral("group"); }
	QString morphismName() const override { return QStringLiteral("homomorphism"); }

protected:
	QChar firstLetter() const override { return QChar('G'); }
};
