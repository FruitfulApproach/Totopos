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
#include <QWheelEvent>
#include <QtMath>
#include "CategoryDialog.h"
#include "Category.h"
#include "Tutor.h"
#include <QCheckBox>

namespace
{
	const QString kCustom = QStringLiteral("Custom...");
}

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
	// zoom about the point under the cursor, not the view's centre
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	buildOverlay();
}

SketchView::~SketchView()
{
}

QStringList SketchView::builtInCategories()
{
	// the registry lives with the Category subclasses; the combo only lists it
	return Category::builtInNames();
}

QString SketchView::category() const
{
	return m_category ? m_category->currentText() : QString("BigCat");
}

void SketchView::setCategory(const QString& name)
{
	// Selects an entry of the combo: a built-in, or a custom category defined
	// earlier. The combo is only a selector; what a category IS lives in the
	// Category subclasses (see Category::createBuiltIn).
	if (m_category == nullptr)
		return;
	int i = m_category->findText(name);
	if (i < 0)
	{
		i = m_category->count() - 1;   // an unknown name is a custom one: before Custom...
		m_category->insertItem(i, name);
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
		"QCheckBox { color: white; background: transparent; }"
		"QComboBox { min-width: 8em; }");
	auto* layout = new QVBoxLayout(m_panel);
	layout->setContentsMargins(10, 8, 10, 8);
	layout->setSpacing(6);

	auto* row = new QHBoxLayout();
	row->setSpacing(6);
	row->addWidget(new QLabel("Category:", m_panel));
	m_category = new QComboBox(m_panel);
	m_category->addItems(builtInCategories());
	m_category->addItem(kCustom);   // last: defines a new one through the dialog
	m_category->setCurrentIndex(0);
	row->addWidget(m_category);
	layout->addLayout(row);

	// tutor mode: guided interactions coach with remarks and an arrow; off,
	// they run quietly. Always here, so it can be switched back on.
	m_tutor = new QCheckBox("Tutor mode", m_panel);
	m_tutor->setToolTip("Guided actions (Define a product...) explain each step with remarks and an arrow. Untick to run them quietly.");
	m_tutor->setChecked(Tutor::isEnabled());
	connect(m_tutor, &QCheckBox::toggled, this, [](bool on) { Tutor::setEnabled(on); });
	layout->addWidget(m_tutor);
	// more controls go here, one row each

	connect(m_category, &QComboBox::currentIndexChanged, this, &SketchView::onCategoryPicked);

	m_panel->adjustSize();
	m_panel->setMaximumHeight(0);
	m_panel->hide();

	m_anim = new QPropertyAnimation(m_panel, "maximumHeight", this);
	m_anim->setDuration(kAnimMs);
	m_anim->setEasingCurve(QEasingCurve::OutCubic);
	// a maximum height only CAPS the widget; the panel must be resized to follow it
	connect(m_anim, &QPropertyAnimation::valueChanged, this, [this](const QVariant& v) {
		m_panel->resize(m_panel->sizeHint().width(), v.toInt());
	});
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
	m_panel->resize(hint.width(), m_open ? hint.height() : m_panel->height());
	m_panel->move(right - m_panel->width(), kMargin + m_toggle->height() + 4);
}

void SketchView::onCategoryPicked(int index)
{
	if (index < 0)
		return;
	if (m_category->itemText(index) == kCustom)
	{
		defineCustomCategory();
		return;
	}
	m_lastCategoryIndex = index;
	emit categoryChanged(m_category->itemText(index));
}

void SketchView::defineCustomCategory()
{
	CategoryDialog dialog(window());
	if (dialog.exec() != QDialog::Accepted || dialog.name().isEmpty())
	{
		// back to what was selected, quietly
		const QSignalBlocker block(m_category);
		m_category->setCurrentIndex(m_lastCategoryIndex);
		return;
	}
	const QString name = dialog.name();
	int i = m_category->findText(name);
	if (i < 0)
	{
		i = m_category->count() - 1;   // before Custom...
		m_category->insertItem(i, name);
	}
	emit categoryDefined(name, dialog.properties());
	m_category->setCurrentIndex(i);   // -> onCategoryPicked -> categoryChanged
}

void SketchView::resizeEvent(QResizeEvent* event)
{
	QGraphicsView::resizeEvent(event);
	placeOverlay();
}

void SketchView::setZoom(qreal factor)
{
	factor = qBound(0.1, factor, 8.0);
	if (qFuzzyCompare(factor, m_zoom))
		return;
	// scale RELATIVE to the current transform so the anchor (cursor) holds still
	scale(factor / m_zoom, factor / m_zoom);
	m_zoom = factor;
}

void SketchView::wheelEvent(QWheelEvent* event)
{
	// one notch = 120 units; trackpads deliver finer steps, so scale by the amount
	const qreal notches = event->angleDelta().y() / 120.0;
	if (qFuzzyIsNull(notches))
	{
		QGraphicsView::wheelEvent(event);
		return;
	}
	setZoom(m_zoom * qPow(1.15, notches));
	event->accept();
}
