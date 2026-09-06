#include "SketchView.h"

#include <QToolButton>
#include <QFrame>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QResizeEvent>

namespace
{
	const int kMargin = 8;
	const int kAnimMs = 120;   // snappy: the panel is a control, not a show
}

SketchView::SketchView(QWidget* parent)
	: QGraphicsView(parent)
{
	setRenderHint(QPainter::Antialiasing, true);
	setDragMode(QGraphicsView::RubberBandDrag);
	buildOverlay();
}

SketchView::~SketchView()
{
}

QStringList SketchView::builtInCategories()
{
	// BigCat first: the default, the category of (possibly large) categories
	return { "BigCat", "Cat", "Set", "Ab", "R-Mod", "Mod-R", "Grp", "Ring", "Top", "Vect" };
}

QString SketchView::category() const
{
	return m_category ? m_category->currentText() : QString("BigCat");
}

void SketchView::setCategory(const QString& name)
{
	if (m_category == nullptr)
		return;
	int i = m_category->findText(name);
	if (i < 0)
	{
		m_category->addItem(name);
		i = m_category->count() - 1;
	}
	m_category->setCurrentIndex(i);
}

void SketchView::buildOverlay()
{
	// Both widgets are children of the view itself (not the viewport), created
	// after it, so they sit above the canvas and do not scroll with it. Each is
	// only as large as its contents: the rest of the view stays click-through.
	m_toggle = new QToolButton(this);
	m_toggle->setText(QString(QChar(0x2261)));   // three bars
	m_toggle->setToolTip("sketch controls");
	m_toggle->setCursor(Qt::PointingHandCursor);
	m_toggle->setCheckable(true);
	m_toggle->setFixedSize(34, 34);
	m_toggle->setStyleSheet(
		"QToolButton { background: rgba(30, 32, 44, 200); color: white; border: 1px solid rgba(255,255,255,60);"
		" border-radius: 17px; font-size: 18px; }"
		"QToolButton:hover { background: rgba(60, 64, 90, 230); }"
		"QToolButton:checked { background: rgba(99, 102, 241, 230); }");
	connect(m_toggle, &QToolButton::clicked, this, &SketchView::toggleMenu);

	m_panel = new QFrame(this);
	m_panel->setObjectName("sketchPanel");
	m_panel->setAttribute(Qt::WA_StyledBackground, true);
	m_panel->setStyleSheet(
		"QFrame#sketchPanel { background: rgba(30, 32, 44, 210); border: 1px solid rgba(255,255,255,60); border-radius: 10px; }"
		"QLabel { color: white; background: transparent; }"
		"QComboBox { min-width: 8em; }");
	auto* layout = new QVBoxLayout(m_panel);
	layout->setContentsMargins(10, 8, 10, 8);
	layout->setSpacing(6);

	auto* row = new QHBoxLayout();
	row->setSpacing(6);
	row->addWidget(new QLabel("Category:", m_panel));
	m_category = new QComboBox(m_panel);
	m_category->addItems(builtInCategories());
	m_category->setCurrentIndex(0);
	row->addWidget(m_category);
	layout->addLayout(row);
	// more controls go here, one row each

	connect(m_category, &QComboBox::currentTextChanged, this, &SketchView::categoryChanged);

	m_panel->adjustSize();
	m_panel->setMaximumHeight(0);
	m_panel->hide();

	m_anim = new QPropertyAnimation(m_panel, "maximumHeight", this);
	m_anim->setDuration(kAnimMs);
	m_anim->setEasingCurve(QEasingCurve::OutCubic);
	connect(m_anim, &QPropertyAnimation::finished, this, [this] {
		if (!m_open)
			m_panel->hide();
	});

	placeOverlay();
	m_toggle->raise();
	m_panel->raise();
}

void SketchView::toggleMenu()
{
	setMenuOpen(!m_open);
}

void SketchView::setMenuOpen(bool open)
{
	if (open == m_open)
		return;
	m_open = open;
	m_toggle->setChecked(open);
	const int full = m_panel->sizeHint().height();
	m_anim->stop();
	m_anim->setStartValue(m_panel->maximumHeight());
	m_anim->setEndValue(open ? full : 0);
	if (open)
	{
		m_panel->show();
		m_panel->raise();
	}
	m_anim->start();
}

void SketchView::placeOverlay()
{
	// top-right, inside the frame; the panel hangs under the toggle, right-aligned
	const int right = width() - kMargin;
	m_toggle->move(right - m_toggle->width(), kMargin);
	const QSize hint = m_panel->sizeHint();
	m_panel->resize(hint.width(), m_panel->maximumHeight() > 0 ? qMin(hint.height(), m_panel->maximumHeight()) : hint.height());
	m_panel->move(right - m_panel->width(), kMargin + m_toggle->height() + 4);
}

void SketchView::resizeEvent(QResizeEvent* event)
{
	QGraphicsView::resizeEvent(event);
	placeOverlay();
}
