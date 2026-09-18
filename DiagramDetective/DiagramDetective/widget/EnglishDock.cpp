#include "widget/EnglishDock.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>

#include "art/DiagramScene.h"
#include "art/Node.h"
#include "widget/ToggleSwitch.h"
#include "core/AppSettings.h"
#include "core/english/Translation.h"

EnglishDock::EnglishDock(QWidget* parent)
	: QDockWidget("English", parent)
{
	setObjectName("englishDock");
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

	auto* body = new QWidget(this);
	auto* layout = new QVBoxLayout(body);
	layout->setContentsMargins(10, 8, 10, 10);
	layout->setSpacing(8);

	// whole diagram, or only what is picked out
	auto* row = new QHBoxLayout();
	row->setSpacing(8);
	auto* label = new QLabel("Read the selection only", body);
	label->setToolTip("On: only what is selected is read out, with the ends of any arrow among it. "
	                  "Off: the whole diagram in front.");
	row->addWidget(label);
	row->addStretch();
	m_selectionOnly = new ToggleSwitch(body);
	m_selectionOnly->setToolTip(label->toolTip());
	m_selectionOnly->setChecked(AppSettings::instance().englishSelectionOnly());
	row->addWidget(m_selectionOnly);
	layout->addLayout(row);
	connect(m_selectionOnly, &QAbstractButton::toggled, this, [this](bool on) {
		AppSettings::instance().setValue(AppSettings::EnglishSelectionOnly, on);
		refresh();
	});

	m_text = new QTextBrowser(body);
	m_text->setOpenExternalLinks(false);
	m_text->setToolTip("What the diagram says, in words. Select it and copy to take it away as text.");
	layout->addWidget(m_text, 1);

	m_note = new QLabel(body);
	m_note->setWordWrap(true);
	m_note->setEnabled(false);
	layout->addWidget(m_note);

	setWidget(body);
	refresh();
}

bool EnglishDock::selectionOnly() const
{
	return m_selectionOnly != nullptr && m_selectionOnly->isChecked();
}

void EnglishDock::setScene(DiagramScene* scene)
{
	if (m_scene != nullptr)
		disconnect(m_scene, nullptr, this, nullptr);
	m_scene = scene;
	if (m_scene != nullptr)
	{
		// what is picked out, and every change to what the diagram says
		connect(m_scene, &QGraphicsScene::selectionChanged, this, &EnglishDock::refresh);
		connect(m_scene, &DiagramScene::statementChanged, this, &EnglishDock::refresh);
		connect(m_scene, &DiagramScene::nodesAdded, this, &EnglishDock::refresh);
		connect(m_scene, &DiagramScene::nodesRemoved, this, &EnglishDock::refresh);
		connect(m_scene, &DiagramScene::chasingChanged, this, &EnglishDock::refresh);
		connect(m_scene, &DiagramScene::commutesChanged, this, &EnglishDock::refresh);
	}
	refresh();
}

void EnglishDock::refresh()
{
	if (m_scene == nullptr)
	{
		m_text->setHtml(QStringLiteral("<p>There is no diagram in front.</p>"));
		m_note->clear();
		return;
	}

	QList<Node*> chosen;
	for (QGraphicsItem* item : m_scene->selectedItems())
		if (auto* node = dynamic_cast<Node*>(item))
			chosen << node;

	const bool only = selectionOnly();
	m_text->setHtml(Translation::describe(m_scene, only ? chosen : QList<Node*>()));
	m_note->setText(only
		? QString("%1 thing%2 selected.").arg(chosen.size()).arg(chosen.size() == 1 ? "" : "s")
		: QString("The whole diagram in %1.")
			.arg(m_scene->ambientCategory() != nullptr ? m_scene->ambientCategory()->id() : QString()));
}
