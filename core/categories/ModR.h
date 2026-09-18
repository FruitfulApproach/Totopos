#pragma once

#include "art/Category.h"

// Right R-modules.
class ModR : public Category
{
	Q_OBJECT
public:
	explicit ModR(QGraphicsItem* parent = nullptr);

	QString builtInName() const override { return QStringLiteral("Mod-R"); }

public:
	QString objectName() const override { return QStringLiteral("R-module"); }
	QString implicitArrowName() const override { return QStringLiteral("0"); }   // the zero map
	QString morphismName() const override { return QStringLiteral("R-linear map"); }

protected:
	QChar firstLetter() const override { return QChar('M'); }
};
