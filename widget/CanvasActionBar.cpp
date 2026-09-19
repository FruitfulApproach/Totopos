#include "widget/CanvasActionBar.h"

#include <QHBoxLayout>
#include <QPushButton>

CanvasActionBar::CanvasActionBar(QWidget* parent)
	: QWidget(parent)
{
	setObjectName("canvasActions");
	// The bar itself takes no mouse: it is a strip of nothing with pills on
	// it, and a click on the nothing belongs to the diagram underneath. The
	// pills are real buttons and take their own clicks.
	setAttribute(Qt::WA_TransparentForMouseEvents, false);
	setStyleSheet(
		"QWidget#canvasActions { background: transparent; }"
		"QPushButton { background: rgba(30, 32, 44, 212); color: white;"
		" border: 1px solid rgba(255,255,255,55); border-radius: 12px;"
		" padding: 5px 14px; font-size: 12px; }"
		"QPushButton:hover { background: rgba(52, 56, 74, 232); }"
		"QPushButton:pressed { background: rgba(22, 24, 34, 240); }");

	m_row = new QHBoxLayout(this);
	m_row->setContentsMargins(0, 0, 0, 0);
	m_row->setSpacing(6);
	m_row->addStretch(1);   // pills sit to the left; the stretch eats the rest
	hide();
}

void CanvasActionBar::setActions(const QList<Action>& actions)
{
	// Nothing to do is the common case and must not cost a rebuild: the bar
	// is asked to refresh on every selection change and every turn of the
	// rule search, which is often.
	if (actions.size() == m_actions.size())
	{
		bool same = true;
		for (int i = 0; i < actions.size() && same; ++i)
			same = actions.at(i).id == m_actions.at(i).id
			    && actions.at(i).text == m_actions.at(i).text;
		if (same)
			return;
	}
	m_actions = actions;

	// out with the old pills; the trailing stretch stays
	while (m_row->count() > 1)
	{
		QLayoutItem* item = m_row->takeAt(0);
		if (QWidget* widget = item->widget())
			widget->deleteLater();
		delete item;
	}

	for (int i = 0; i < m_actions.size(); ++i)
	{
		const Action& action = m_actions.at(i);
		auto* pill = new QPushButton(action.text, this);
		pill->setCursor(Qt::PointingHandCursor);
		if (!action.tooltip.isEmpty())
			pill->setToolTip(action.tooltip);
		const QString id = action.id;
		connect(pill, &QPushButton::clicked, this, [this, id] { emit triggered(id); });
		m_row->insertWidget(i, pill, 0);
	}

	setVisible(!m_actions.isEmpty());
	adjustSize();
	emit changed();
}
