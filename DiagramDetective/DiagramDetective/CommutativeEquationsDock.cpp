#include "CommutativeEquationsDock.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>

#include "DiagramScene.h"
#include "ToggleSwitch.h"
#include "AppSettings.h"

CommutativeEquationsDock::CommutativeEquationsDock(QWidget* parent)
	: QDockWidget("Equations", parent)
{
	setObjectName("equationsDock");
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

	auto* body = new QWidget(this);
	auto* layout = new QVBoxLayout(body);
	layout->setContentsMargins(10, 8, 10, 10);
	layout->setSpacing(8);

	// how a composite is written
	auto* row = new QHBoxLayout();
	row->setSpacing(8);
	auto* label = new QLabel(QString("Write %1 rather than gf").arg(QString("g %1 f").arg(QChar(0x2218))), body);
	label->setToolTip(QString("A composite as g %1 f, read right to left, or written side by side as gf.")
		.arg(QChar(0x2218)));
	row->addWidget(label);
	row->addStretch();
	m_ring = new ToggleSwitch(body);
	m_ring->setToolTip(label->toolTip());
	m_ring->setChecked(AppSettings::instance().composeWithRing());
	row->addWidget(m_ring);
	layout->addLayout(row);
	connect(m_ring, &QAbstractButton::toggled, this, [this](bool on) {
		AppSettings::instance().setValue(AppSettings::ComposeWithRing, on);
		refresh();
	});

	m_note = new QLabel(body);
	m_note->setWordWrap(true);
	m_note->setEnabled(false);
	layout->addWidget(m_note);

	m_equations = new QListWidget(body);
	m_equations->setAlternatingRowColors(true);
	m_equations->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_equations->setToolTip("Each line is a pair of paths with the same two ends. A commuting diagram "
	                        "says they are the same arrow.");
	layout->addWidget(m_equations, 1);

	setWidget(body);
	refresh();
}

void CommutativeEquationsDock::setScene(DiagramScene* scene)
{
	if (m_scene != nullptr)
		disconnect(m_scene, nullptr, this, nullptr);
	m_scene = scene;
	if (m_scene != nullptr)
	{
		// the statement changes on every add, delete, undo, redo and toggle,
		// which is exactly when the equations change too
		connect(m_scene, &DiagramScene::statementChanged, this, &CommutativeEquationsDock::refresh);
		connect(m_scene, &DiagramScene::chasingChanged, this, &CommutativeEquationsDock::refresh);
	}
	refresh();
}

void CommutativeEquationsDock::refresh()
{
	m_equations->clear();
	if (m_scene == nullptr)
	{
		m_note->setText("No diagram.");
		return;
	}
	if (!m_scene->commutes())
	{
		m_note->setText("This diagram is not asserted to commute, so it says nothing about its paths. "
		                "Turn Commutes on in the sketch panel to read the equations.");
		return;
	}
	if (!m_scene->cycleError().isEmpty())
	{
		m_note->setText("There is a cycle in the diagram, so its paths cannot be read as equations. "
		                "The cycle is drawn in red.");
		return;
	}

	const QStringList lines = m_scene->commutingEquations(m_ring->isChecked());
	m_equations->addItems(lines);
	if (lines.isEmpty())
		m_note->setText("Nothing yet: an equation needs two different paths between the same two "
		                "objects. Draw a square, or any two ways round.");
	else
		m_note->setText(QString("%1 equation%2 — every pair of paths with the same two ends.")
			.arg(lines.size()).arg(lines.size() == 1 ? "" : "s"));
}
