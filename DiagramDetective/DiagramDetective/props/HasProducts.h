#pragma once

#include "CategoryProp.h"
#include "../Tutor.h"

// The category has products. From a category's right-click menu it offers
// "Define a product..." and then TEACHES the user: pick the factors in order,
// press Done, and the product A x B appears with its projections.
class HasProducts : public CategoryProp, public Tutor
{
	Q_OBJECT

public:
	explicit HasProducts(Category* category = nullptr) : CategoryProp(category) {}
	static QString Key() { return QStringLiteral("hasProducts"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Has products"); }
	QString description() const override { return QStringLiteral("Every pair of objects has a product A x B with its projections."); }

	void categoryContextMenu(QMenu& menu, Category* category) override;
	QString tutorTitle() const override { return QStringLiteral("Define a product"); }

protected:
	void onBegin(TutorSession& s) override;
	bool onPick(TutorSession& s, Node* node) override;
	bool onDone(TutorSession& s) override;
};
