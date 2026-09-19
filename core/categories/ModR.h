#pragma once

#include "art/Category.h"

// Right R-modules.
class ModR : public Category
{
	Q_OBJECT
public:
	explicit ModR(QGraphicsItem* parent = nullptr);

	QString builtInName() const override { return QStringLiteral("Mod-R"); }

	// An object of Mod-R is an R-MODULE, not a bag of elements that happens
	// to sit here: it has a zero and its elements can be scaled (see RModule).
	Object* makeObject(const QString& name) override;

public:
	QString objectName() const override { return QStringLiteral("R-module"); }
	QString implicitArrowName() const override { return QStringLiteral("0"); }   // the zero map
	QString morphismName() const override { return QStringLiteral("R-linear map"); }

protected:
	QChar firstLetter() const override { return QChar('M'); }
};
