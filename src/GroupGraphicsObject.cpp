#include "GroupGraphicsObject.hpp"
#include "BasicGraphicsScene.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeGraphicsObject.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtNodes/DataFlowGraphModel>

using QtNodes::BasicGraphicsScene;
using QtNodes::ConnectionId;
using QtNodes::DataFlowGraphModel;
using QtNodes::GroupGraphicsObject;
using QtNodes::NodeConnectionInteraction;
using QtNodes::NodeGraphicsObject;
using QtNodes::NodeGroup;

IconGraphicsItem::IconGraphicsItem(QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{}

IconGraphicsItem::IconGraphicsItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
    _scaleFactor = _iconSize / pixmap.size().width();
    setScale(_scaleFactor);
}

double IconGraphicsItem::scaleFactor() const
{
    return _scaleFactor;
}

GroupGraphicsObject::GroupGraphicsObject(BasicGraphicsScene &scene, NodeGroup &nodeGroup)
    : _scene(scene)
    , _group(nodeGroup)
    , _possibleChild(nullptr)
    , _locked(false)
{
    setRect(0, 0, _defaultWidth, _defaultHeight);

    _lockedGraphicsItem = new IconGraphicsItem(_lockedIcon, this);
    _unlockedGraphicsItem = new IconGraphicsItem(_unlockedIcon, this);

    _scene.addItem(this);

    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);

    _currentFillColor = kUnlockedFillColor;
    _currentBorderColor = kUnselectedBorderColor;

    _borderPen = QPen(_currentBorderColor, 1.0, Qt::PenStyle::DashLine);

    setZValue(-_groupAreaZValue);

    setAcceptHoverEvents(true);
}

GroupGraphicsObject::~GroupGraphicsObject()
{
    _scene.removeItem(this);
}

NodeGroup &GroupGraphicsObject::group()
{
    return _group;
}

NodeGroup const &GroupGraphicsObject::group() const
{
    return _group;
}

QRectF GroupGraphicsObject::boundingRect() const
{
    QRectF ret{};
    for (auto &node : _group.childNodes()) {
        ret |= node->mapRectToScene(node->boundingRect());
    }
    if (_possibleChild) {
        ret |= _possibleChild->mapRectToScene(_possibleChild->boundingRect());
    }
    return mapRectFromScene(ret.marginsAdded(_margins));
}

void GroupGraphicsObject::setFillColor(const QColor &color)
{
    _currentFillColor = color;
    update();
}

void GroupGraphicsObject::setBorderColor(const QColor &color)
{
    _currentBorderColor = color;
    _borderPen.setColor(_currentBorderColor);
}

void GroupGraphicsObject::moveConnections()
{
    for (auto &node : group().childNodes()) {
        node->moveConnections();
    }
}

void GroupGraphicsObject::moveNodes(const QPointF &offset)
{
    for (auto &node : group().childNodes()) {
        auto newPosition = QPointF(node->x() + offset.x(), node->y() + offset.y());
        node->setPos(newPosition);
        node->update();
    }
}

void GroupGraphicsObject::lock(bool locked)
{
    for (auto &node : _group.childNodes()) {
        node->lock(locked);
    }
    _lockedGraphicsItem->setVisible(locked);
    _unlockedGraphicsItem->setVisible(!locked);
    setFillColor(locked ? kLockedFillColor : kUnlockedFillColor);
    _locked = locked;
    // CICADA: the rectangle must STAY beneath nodes regardless of the
    // lock state. The legacy code flipped Z to +_groupAreaZValue on
    // lock, putting the group ABOVE the nodes and blocking all clicks
    // on the contained items. Use the same negative Z either way so
    // clicks always reach nodes; the visual lock indicator is the
    // padlock icon (made visible above), not a stacking change.
    setZValue(-_groupAreaZValue);
}

bool GroupGraphicsObject::locked() const
{
    return _locked;
}

void GroupGraphicsObject::positionLockedIcon()
{
    _lockedGraphicsItem->setPos(
        boundingRect().topRight()
        + QPointF(-(_roundedBorderRadius + IconGraphicsItem::iconSize()), _roundedBorderRadius));
    _unlockedGraphicsItem->setPos(
        boundingRect().topRight()
        + QPointF(-(_roundedBorderRadius + IconGraphicsItem::iconSize()), _roundedBorderRadius));
}

void GroupGraphicsObject::setHovered(bool hovered)
{
    // CICADA: only override the fill color on hover when the user
    // has NOT picked a custom color via "Change color...". Otherwise
    // every mouse-enter / mouse-leave would clobber the user's choice
    // with the hardcoded kUnlocked*/kLocked* constants.
    if (!_userFillColorOverridden) {
        hovered
            ? _currentFillColor =
                  (locked() ? kLockedHoverColor : kUnlockedHoverColor)
            : _currentFillColor =
                  (locked() ? kLockedFillColor : kUnlockedFillColor);
    }

    for (auto &node : _group.childNodes()) {
        auto ngo = node->nodeScene()->nodeGraphicsObject(node->nodeId());
        ngo->nodeState().setHovered(hovered);
        node->update();
    }
    update();
}

void GroupGraphicsObject::setPossibleChild(QtNodes::NodeGraphicsObject *possibleChild)
{
    _possibleChild = possibleChild;
}

void GroupGraphicsObject::unsetPossibleChild()
{
    _possibleChild = nullptr;
}

std::vector<std::shared_ptr<ConnectionId>> GroupGraphicsObject::connections() const
{
    return _scene.connectionsWithinGroup(group().id());
}

void GroupGraphicsObject::setPosition(const QPointF &position)
{
    QPointF diffPos = position - scenePos();
    moveNodes(diffPos);
    moveConnections();
}

void GroupGraphicsObject::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    setHovered(true);
}

void GroupGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    setHovered(false);
}

void GroupGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // CICADA: belt-and-suspenders with contains(). If we ever do
    // receive a press here AND a member node sits at that scene
    // position, ignore the event so Qt's grabber doesn't latch onto
    // the group. contains() above is what stops Qt from delivering
    // this event in the first place in normal scenarios.
    if (scene()) {
        for (QGraphicsItem *item : scene()->items(event->scenePos())) {
            if (item == this) {
                continue;
            }
            if (qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
                event->ignore();
                return;
            }
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

bool GroupGraphicsObject::contains(const QPointF &point) const
{
    // Convert the local point to scene coords and check every member
    // node. If the point lies inside any node's scene rect, return
    // false — Qt's hit-test will then skip the group at this point
    // and pick the node as the topmost item.
    QPointF const scenePoint = const_cast<GroupGraphicsObject *>(this)
                                   ->mapToScene(point);
    for (NodeGraphicsObject *child : _group.childNodes()) {
        if (child &&
            child->mapRectToScene(child->boundingRect()).contains(scenePoint)) {
            qDebug() << "GroupGO::contains -> false (node under cursor)";
            return false;
        }
    }
    return QGraphicsRectItem::contains(point);
}

QPainterPath GroupGraphicsObject::shape() const
{
    // Build a shape that EXCLUDES every member node's rect from the
    // group's bounding rectangle. Qt code paths that consult shape()
    // (e.g. some collision / hover queries) then treat the area over
    // each node as NOT being part of the group — Qt's hit-test
    // delivers the press to the node instead.
    QPainterPath outer;
    outer.addRect(boundingRect());
    QPainterPath holes;
    for (NodeGraphicsObject *child : _group.childNodes()) {
        if (!child) {
            continue;
        }
        QRectF const nodeSceneRect =
            child->mapRectToScene(child->boundingRect());
        QRectF const nodeLocalRect = mapRectFromScene(nodeSceneRect);
        holes.addRect(nodeLocalRect);
    }
    return outer.subtracted(holes);
}

void GroupGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseMoveEvent(event);
    if (event->lastPos() != event->pos()) {
        auto diff = event->pos() - event->lastPos();
        moveNodes(diff);
        moveConnections();
    }
}

void GroupGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseDoubleClickEvent(event);
    lock(!locked());
}

void GroupGraphicsObject::paint(QPainter *painter,
                                const QStyleOptionGraphicsItem *option,
                                QWidget *widget)
{
    Q_UNUSED(widget);
    prepareGeometryChange();
    setRect(boundingRect());
    positionLockedIcon();
    painter->setClipRect(option->exposedRect);
    painter->setBrush(_currentFillColor);

    setBorderColor(isSelected() ? kSelectedBorderColor : kUnselectedBorderColor);
    painter->setPen(_borderPen);

    painter->drawRoundedRect(rect(), _roundedBorderRadius, _roundedBorderRadius);
}
