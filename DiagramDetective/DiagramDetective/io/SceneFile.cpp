#include "SceneFile.h"

#include <QFile>
#include <QDataStream>
#include <QSaveFile>
#include "../DiagramScene.h"
#include "../Category.h"
#include "../Arrow.h"
#include "../Functor.h"
#include "../history/SceneHistory.h"
#include "../history/Mementos.h"
#include "../props/MapsElements.h"

namespace
{
	const char kMagic[4] = { 'D', 'D', 'G', 'M' };
	const quint16 kVersion = 14;   // 13: which pieces are exact. 14: what it is, written on the outside   // 2: Exists such, hypotheses, commuting. 3: rounding, image links, mapping settings. 4: bends

	QString kindOf(Node* node)
	{
		if (dynamic_cast<Functor*>(node) != nullptr)  return QStringLiteral("Functor");
		if (dynamic_cast<Arrow*>(node) != nullptr)    return QStringLiteral("Arrow");
		if (dynamic_cast<Category*>(node) != nullptr) return QStringLiteral("Category");
		return QStringLiteral("Object");
	}

	// a colour that is not valid means "none": no fill, no border
	void writeStyle(QDataStream& out, Node* node)
	{
		const QBrush fill = node->fill();
		const QPen border = node->border();
		out << (fill.style() == Qt::NoBrush ? QColor() : fill.color());
		out << (border.style() == Qt::NoPen ? QColor() : border.color());
		out << qreal(border.widthF());
	}

	void readStyle(QDataStream& in, Node* node)
	{
		QColor fill, border;
		qreal width = 1.5;
		in >> fill >> border >> width;
		node->setFill(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
		node->setBorder(border.isValid() ? QPen(border, width) : QPen(Qt::NoPen));
	}

	// One node and everything under it. Arrows are written where they sit but
	// carry the paths of their two ends, and are built in a second pass once
	// every object exists.
	void writeNode(QDataStream& out, Node* node)
	{
		out << kindOf(node) << node->id() << node->pos() << qreal(node->zValue());
		writeStyle(out, node);
		out << node->existsSuch() << node->isHypothesis();
		out << node->commutesInComponent()
		    << node->rowsExactInComponent() << node->columnsExactInComponent();
		// what it is the image of, so a live functor knows its own work again
		out << qreal(node->cornerRadius())
		    << node->data(MapsElements::ImageFunctorKey).toString()
		    << node->data(MapsElements::ImageSourceKey).toString()
		    << node->data(MapsElements::MappingIdKey).toString()
		    << node->data(MapsElements::ImageMappingKey).toString();

		// a label made out of other labels, and which nodes those are
		const QList<Node*> sources = node->labelSources();
		out << node->labelPattern() << qint32(sources.size());
		for (Node* source : sources)
			out << source->pathFromRoot() << source->id();

		if (auto* category = dynamic_cast<Category*>(node))
		{
			out << category->properties() << qint32(category->nextObjectIndex()) << qint32(category->nextArrowIndex());
			out << category->rowsExact() << category->columnsExact();
		}
		else if (auto* arrow = dynamic_cast<Arrow*>(node))
		{
			out << arrow->properties();
			out << (arrow->domain() != nullptr ? arrow->domain()->pathFromRoot() : QList<int>());
			out << (arrow->codomain() != nullptr ? arrow->codomain()->pathFromRoot() : QList<int>());

			// how a functor maps, when it is one
			auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key()));
			out << quint8(maps != nullptr && maps->isLive() ? 1 : 0)
			    << quint8(maps != nullptr && maps->imaginesPosition() ? 1 : 0)
			    << quint8(maps != nullptr && maps->reflectsPosition() ? 1 : 0)
			    << quint8(maps != nullptr && maps->imaginesBends() ? 1 : 0)
			    << quint8(maps != nullptr && maps->reflectsBends() ? 1 : 0);
			out << arrow->labelOffset();
			out << quint8(maps != nullptr && maps->isContravariant() ? 1 : 0);
			out << arrow->bends();   // the points its line is pulled through
		}

		QList<Node*> children;
		for (QGraphicsItem* child : node->childItems())
			if (auto* kid = dynamic_cast<Node*>(child))
				children << kid;
		out << qint32(children.size());
		for (Node* child : children)
			writeNode(out, child);
	}

	// a label that names other nodes: they may not all exist yet while the
	// tree is being read, so these are tied up at the end
	struct PendingDerived
	{
		Node* node = nullptr;
		QString pattern;
		QList<QPair<QList<int>, QString>> sources;
	};

	// the node a (path, label) pair names: the path alone cannot name an
	// arrow, which is why the label goes with it
	Node* resolveRef(Node* ambient, const QList<int>& path, const QString& id)
	{
		Node* at = Node::fromPath(ambient, path);
		if (at == nullptr)
			return nullptr;
		if (at->id() == id)
			return at;
		for (QGraphicsItem* child : at->childItems())
			if (auto* node = dynamic_cast<Node*>(child))
				if (node->id() == id)
					return node;
		return nullptr;
	}

	void readDerived(QDataStream& in, QString& pattern, QList<QPair<QList<int>, QString>>& sources)
	{
		qint32 count = 0;
		in >> pattern >> count;
		if (in.status() != QDataStream::Ok || count < 0 || count > 10000)
		{
			in.setStatus(QDataStream::ReadCorruptData);
			return;
		}
		for (qint32 i = 0; i < count; ++i)
		{
			QList<int> path;
			QString id;
			in >> path >> id;
			sources.append(qMakePair(path, id));
		}
	}

	struct PendingArrow
	{
		Node* parent = nullptr;
		QString kind, id;
		QStringList props;
		QPointF pos;
		qreal z = 0;
		QColor fill, border;
		qreal width = 1.5;
		QList<int> domain, codomain;
		bool existsSuch = false;
		bool hypothesis = false;
		bool componentCommutes = true;
		bool componentRows = false, componentColumns = false;
		qreal radius = 5.0;
		QString imageFunctor, imageSource;
		QString mappingId, imageMapping;
		quint8 live = 1, reflect = 1, direction = 0;
		quint8 imagineBends = 1, reflectBends = 0, contravariant = 0;
		QPointF labelOffset;
		QList<QPointF> bends;
		QString pattern;
		QList<QPair<QList<int>, QString>> patternSources;
	};

	void readNode(QDataStream& in, Node* parent, QList<PendingArrow>& pending,
	              QList<PendingDerived>& derived, quint16 version)
	{
		QString kind, id;
		QPointF pos;
		qreal z = 0;
		in >> kind >> id >> pos >> z;

		QColor fill, border;
		qreal width = 1.5;
		in >> fill >> border >> width;

		bool existsSuch = false;
		bool hypothesis = false;
		if (version >= 2)
			in >> existsSuch >> hypothesis;
		bool componentCommutes = true;
		bool componentRows = false, componentColumns = false;
		if (version >= 12)
			in >> componentCommutes;
		if (version >= 13)
			in >> componentRows >> componentColumns;

		qreal radius = 5.0;
		QString imageFunctor, imageSource, mappingId, imageMapping;
		if (version >= 3)
			in >> radius >> imageFunctor >> imageSource;
		if (version >= 8)
			in >> mappingId >> imageMapping;

		QString pattern;
		QList<QPair<QList<int>, QString>> patternSources;
		if (version >= 5)
			readDerived(in, pattern, patternSources);

		Node* node = nullptr;
		if (kind == "Arrow" || kind == "Functor")
		{
			PendingArrow arrow;
			arrow.parent = parent;
			arrow.kind = kind;
			arrow.id = id;
			arrow.pos = pos;
			arrow.z = z;
			arrow.fill = fill;
			arrow.border = border;
			arrow.width = width;
			arrow.existsSuch = existsSuch;
			arrow.hypothesis = hypothesis;
			arrow.componentCommutes = componentCommutes;
			arrow.componentRows = componentRows;
			arrow.componentColumns = componentColumns;
			arrow.radius = radius;
			arrow.imageFunctor = imageFunctor;
			arrow.imageSource = imageSource;
			arrow.mappingId = mappingId;
			arrow.imageMapping = imageMapping;
			arrow.pattern = pattern;
			arrow.patternSources = patternSources;
			in >> arrow.props >> arrow.domain >> arrow.codomain;
			if (version >= 3)
				in >> arrow.live >> arrow.reflect >> arrow.direction;
			if (version >= 6)
				in >> arrow.imagineBends >> arrow.reflectBends;
			if (version >= 7)
				in >> arrow.labelOffset;
			if (version >= 9)
				in >> arrow.contravariant;
			if (version >= 4)
				in >> arrow.bends;
			pending << arrow;
			// an arrow holds nothing but its own label
			qint32 count = 0;
			in >> count;
			if (in.status() != QDataStream::Ok || count < 0 || count > 100000)
			{
				in.setStatus(QDataStream::ReadCorruptData);
				return;
			}
			for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
				readNode(in, parent, pending, derived, version);
			return;
		}

		if (kind == "Category")
		{
			auto* category = new Category(id, parent);
			QStringList props;
			qint32 nextObject = 0, nextArrow = 0;
			in >> props >> nextObject >> nextArrow;
			category->setProperties(props);
			category->setNextObjectIndex(nextObject);
			category->setNextArrowIndex(nextArrow);
			if (version >= 10)
			{
				bool rows = false, columns = false;
				in >> rows >> columns;
				category->setRowsExact(rows);
				category->setColumnsExact(columns);
			}
			node = category;
		}
		else
		{
			node = new Object(id, parent);
		}

		node->setPos(pos);
		node->setZValue(z);
		node->setFill(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
		node->setBorder(border.isValid() ? QPen(border, width) : QPen(Qt::NoPen));
		node->setExistsSuch(existsSuch);
		node->setHypothesis(hypothesis);
		node->setCommutesInComponent(componentCommutes);
		node->setRowsExactInComponent(componentRows);
		node->setColumnsExactInComponent(componentColumns);
		node->setCornerRadius(radius);
		if (!imageFunctor.isEmpty())
		{
			node->setData(MapsElements::ImageFunctorKey, imageFunctor);
			node->setData(MapsElements::ImageSourceKey, imageSource);
		}
		if (!mappingId.isEmpty())
			node->setData(MapsElements::MappingIdKey, mappingId);
		if (!imageMapping.isEmpty())
			node->setData(MapsElements::ImageMappingKey, imageMapping);

		if (!pattern.isEmpty())
			derived.append(PendingDerived{ node, pattern, patternSources });

		qint32 count = 0;
		in >> count;
		// a count no diagram could have means the stream is misread: stop,
		// rather than build a million nodes out of noise
		if (in.status() != QDataStream::Ok || count < 0 || count > 100000)
		{
			in.setStatus(QDataStream::ReadCorruptData);
			return;
		}
		for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
			readNode(in, node, pending, derived, version);
	}
}

QString SceneFile::extension() { return QStringLiteral("totopos"); }
QString SceneFile::filter() { return QStringLiteral("Totopos diagram (*.totopos)"); }

SceneFile::Heading SceneFile::peek(const QString& path)
{
	Heading heading;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return heading;
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_6_0);

	char magic[4] = { 0, 0, 0, 0 };
	in.readRawData(magic, 4);
	if (memcmp(magic, kMagic, 4) != 0)
		return heading;
	quint16 version = 0;
	in >> version;
	if (version < 14)
	{
		heading.valid = true;   // an older file: readable, but it does not say
		return heading;
	}
	quint16 kind = 0;
	in >> kind;
	// The name is a QString: a length, then the text. Read the length by hand
	// and refuse anything absurd - a damaged file must not become an attempt
	// to allocate gigabytes.
	quint32 bytes = 0;
	in >> bytes;
	if (bytes != 0xFFFFFFFFu && bytes > 0)
	{
		if (bytes > 8192 || in.status() != QDataStream::Ok)
			return heading;   // not a heading we believe: treat the file as unreadable
		QByteArray utf16(int(bytes), Qt::Uninitialized);
		if (in.readRawData(utf16.data(), int(bytes)) != int(bytes))
			return heading;
		// QDataStream writes a QString as UTF-16 big-endian, whatever the machine
		heading.name.reserve(int(bytes / 2));
		for (int i = 0; i + 1 < int(bytes); i += 2)
			heading.name.append(QChar(ushort((uchar(utf16.at(i)) << 8) | uchar(utf16.at(i + 1)))));
	}
	heading.kind = int(kind);
	heading.valid = true;
	return heading;
}

bool SceneFile::save(DiagramScene* scene, const QString& path, QString* error)
{
	if (scene == nullptr || scene->ambientCategory() == nullptr)
	{
		if (error != nullptr) *error = QStringLiteral("There is no diagram to save.");
		return false;
	}

	QByteArray body;
	{
		QDataStream out(&body, QIODevice::WriteOnly);
		out.setVersion(QDataStream::Qt_6_0);
		writeNode(out, scene->ambientCategory());
		out << scene->commutes() << scene->isChasing();
		out << qint32(scene->statementKind()) << scene->statementName();

		// the history: what was DONE, never where anything was dragged to
		SceneHistory* history = scene->history();
		QList<Memento*> steps;
		for (int i = 0; i < history->position(); ++i)
		{
			Memento* memento = history->mementos().at(i);
			if (!memento->isPureGraphical())
				steps << memento;
		}
		out << qint32(steps.size());
		for (Memento* memento : steps)
			out << quint16(memento->typeTag()) << memento->describe() << memento->payload();
	}

	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly))
	{
		if (error != nullptr) *error = file.errorString();
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_6_0);
	out.writeRawData(kMagic, 4);
	out << kVersion;
	// in the clear, so a library can be listed without unpacking anything
	out << quint16(scene->statementKind()) << scene->statementName();
	out << qCompress(body, 9);   // binary, and small
	if (!file.commit())
	{
		if (error != nullptr) *error = file.errorString();
		return false;
	}
	return true;
}

bool SceneFile::load(DiagramScene* scene, const QString& path, QString* error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		if (error != nullptr) *error = file.errorString();
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_6_0);

	char magic[4] = { 0, 0, 0, 0 };
	in.readRawData(magic, 4);
	if (memcmp(magic, kMagic, 4) != 0)
	{
		if (error != nullptr) *error = QStringLiteral("That is not a Diagram Detective file.");
		return false;
	}
	quint16 version = 0;
	in >> version;
	if (version > kVersion)
	{
		if (error != nullptr) *error = QStringLiteral("That file was written by a newer version.");
		return false;
	}
	if (version >= 14)
	{
		quint16 kind = 0;
		QString name;
		in >> kind >> name;   // read again from the body below; this copy is for listing
	}
	QByteArray compressed;
	in >> compressed;
	const QByteArray body = qUncompress(compressed);
	if (body.isEmpty())
	{
		if (error != nullptr) *error = QStringLiteral("The file is empty or damaged.");
		return false;
	}

	QDataStream data(body);
	data.setVersion(QDataStream::Qt_6_0);

	// the ambient category first: it decides what the canvas is
	QString kind, id;
	QPointF pos;
	qreal z = 0;
	data >> kind >> id >> pos >> z;
	QColor fill, border;
	qreal width = 1.5;
	data >> fill >> border >> width;
	bool ambientExistsSuch = false, ambientHypothesis = false;
	if (version >= 2)
		data >> ambientExistsSuch >> ambientHypothesis;
	bool ambientComponentCommutes = true;
	bool ambientRows = false, ambientColumns = false;
	if (version >= 12)
		data >> ambientComponentCommutes;
	if (version >= 13)
		data >> ambientRows >> ambientColumns;
	qreal ambientRadius = 5.0;
	QString ambientImageFunctor, ambientImageSource;
	if (version >= 3)
		data >> ambientRadius >> ambientImageFunctor >> ambientImageSource;
	// writeNode writes these for EVERY node, the ambient one included; reading
	// the ambient by hand and leaving them out put every later field two
	// strings early, and the child count that came out of that was garbage
	QString ambientMappingId, ambientImageMapping;
	if (version >= 8)
		data >> ambientMappingId >> ambientImageMapping;
	QString ambientPattern;
	QList<QPair<QList<int>, QString>> ambientPatternSources;
	if (version >= 5)
		readDerived(data, ambientPattern, ambientPatternSources);
	QStringList props;
	qint32 nextObject = 0, nextArrow = 0;
	data >> props >> nextObject >> nextArrow;

	scene->clearDiagram();
	scene->setAmbientCategory(id);
	Category* ambient = scene->ambientCategory();
	if (ambient == nullptr)
	{
		if (error != nullptr) *error = QStringLiteral("The file names a category that cannot be made.");
		return false;
	}
	ambient->setProperties(props);
	ambient->setNextObjectIndex(nextObject);
	ambient->setNextArrowIndex(nextArrow);
	if (version >= 10)
	{
		bool rows = false, columns = false;
		data >> rows >> columns;
		ambient->setRowsExact(rows);
		ambient->setColumnsExact(columns);
	}
	if (fill.isValid()) ambient->setFill(QBrush(fill));
	if (border.isValid()) ambient->setBorder(QPen(border, width));

	QList<PendingArrow> pending;
	QList<PendingDerived> derived;
	qint32 count = 0;
	data >> count;
	if (data.status() != QDataStream::Ok || count < 0 || count > 100000)
	{
		if (error != nullptr) *error = QStringLiteral("The file does not add up: its contents cannot be read.");
		return false;
	}
	for (qint32 i = 0; i < count && data.status() == QDataStream::Ok; ++i)
		readNode(data, ambient, pending, derived, version);
	if (data.status() != QDataStream::Ok)
	{
		if (error != nullptr) *error = QStringLiteral("The file is damaged part way through.");
		return false;
	}

	// now every object exists, so the arrows can find their ends
	for (const PendingArrow& p : pending)
	{
		Node* from = Node::fromPath(ambient, p.domain);
		Node* to = Node::fromPath(ambient, p.codomain);
		if (from == nullptr || to == nullptr)
			continue;   // an end that is no longer there: the arrow is dropped
		// stamped first: MapsElements reads this in its constructor
		Arrow* arrow = nullptr;
		auto* domCat = dynamic_cast<Category*>(from);
		auto* codCat = dynamic_cast<Category*>(to);
		if (p.kind == "Functor" && domCat != nullptr && codCat != nullptr)
			arrow = new Functor(p.id, domCat, codCat, p.parent);
		else
			arrow = new Arrow(p.id, from, to, p.parent);
		if (!p.mappingId.isEmpty())
			arrow->setData(MapsElements::MappingIdKey, p.mappingId);
		if (!p.imageMapping.isEmpty())
			arrow->setData(MapsElements::ImageMappingKey, p.imageMapping);
		arrow->setZValue(p.z);
		arrow->setProperties(p.props);
		arrow->addProperty(MapsElements::Key());   // every arrow can carry elements over
		if (p.fill.isValid()) arrow->setFill(QBrush(p.fill));
		arrow->setBorder(p.border.isValid() ? QPen(p.border, p.width) : QPen(Qt::black, 1.5));
		arrow->setExistsSuch(p.existsSuch);
		arrow->setHypothesis(p.hypothesis);
		arrow->setCommutesInComponent(p.componentCommutes);
		arrow->setRowsExactInComponent(p.componentRows);
		arrow->setColumnsExactInComponent(p.componentColumns);
		arrow->setCornerRadius(p.radius);
		arrow->setBends(p.bends);
		arrow->setLabelOffset(p.labelOffset);
		if (!p.pattern.isEmpty())
			derived.append(PendingDerived{ arrow, p.pattern, p.patternSources });
		if (!p.imageFunctor.isEmpty())
		{
			arrow->setData(MapsElements::ImageFunctorKey, p.imageFunctor);
			arrow->setData(MapsElements::ImageSourceKey, p.imageSource);
		}

		// how it maps, if it maps at all. Live goes last: switching it on is
		// what brings the image up to date, and by now everything exists.
		if (auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())))
		{
			// first: it decides which images this mapping recognises as its own
			maps->setMappingId(p.mappingId);
			maps->setImaginesPosition(p.reflect != 0);
			maps->setReflectsPosition(p.direction == 1);
			maps->setContravariant(p.contravariant != 0);
			maps->setImaginesBends(p.imagineBends != 0);
			maps->setReflectsBends(p.reflectBends != 0);
			maps->setLive(p.live != 0);
		}
	}

	// now that every object and every arrow is there, the labels built out of
	// other labels can find what they are built from
	for (const PendingDerived& entry : derived)
	{
		QList<Node*> sources;
		for (const auto& ref : entry.sources)
			if (Node* source = resolveRef(ambient, ref.first, ref.second))
				sources << source;
		if (entry.node != nullptr && !sources.isEmpty())
			entry.node->setDerivedLabel(entry.pattern, sources);
	}

	// what the diagram claims, and whether a chase was under way
	if (version >= 2)
	{
		bool commutes = false, chasing = false;
		data >> commutes >> chasing;
		scene->setCommutes(commutes);
		scene->setChasing(chasing);
		if (version >= 11)
		{
			qint32 kind = 0;
			QString name;
			data >> kind >> name;
			scene->setStatementName(name);
			scene->setStatementKind(DiagramScene::StatementKind(kind));
		}
	}

	// the history as it was: steps that can be read, not walked back
	qint32 steps = 0;
	data >> steps;
	for (qint32 i = 0; i < steps; ++i)
	{
		quint16 tag = 0;
		QString description;
		QByteArray payload;
		data >> tag >> description >> payload;
		scene->history()->record(new SealedStep(description, tag, payload, false));
	}

	ambient->refreshDepthAppearance();
	ambient->refreshFrame();
	return true;
}
