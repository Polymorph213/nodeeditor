#pragma once

#include <utility>

#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsObject>

#include "ConnectionState.hpp"
#include "Definitions.hpp"

class QGraphicsSceneMouseEvent;

namespace QtNodes {

class AbstractGraphModel;
class BasicGraphicsScene;

/// Graphic Object for connection. Adds itself to scene
class NODE_EDITOR_PUBLIC ConnectionGraphicsObject : public QGraphicsObject
{
    Q_OBJECT
public:
    // Needed for qgraphicsitem_cast
    enum { Type = UserType + 2 };

    int type() const override { return Type; }

public:
    ConnectionGraphicsObject(BasicGraphicsScene &scene, ConnectionId const connectionId);

    ~ConnectionGraphicsObject() = default;

public:
    AbstractGraphModel &graphModel() const;

    BasicGraphicsScene *nodeScene() const;

    ConnectionId const &connectionId() const;

    QRectF boundingRect() const override;

    QPainterPath shape() const override;

    QPointF const &endPoint(PortType portType) const;

    QPointF out() const { return _out; }

    QPointF in() const { return _in; }

    std::pair<QPointF, QPointF> pointsC1C2() const;

    void setEndPoint(PortType portType, QPointF const &point);

    /// Updates the position of both ends
    void move();

    ConnectionState const &connectionState() const;

    ConnectionState &connectionState();

protected:
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = 0) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void initializePosition();

    void addGraphicsEffect();

    std::pair<QPointF, QPointF> pointsC1C2Horizontal() const;

    std::pair<QPointF, QPointF> pointsC1C2Vertical() const;

private:
    ConnectionId _connectionId;

    AbstractGraphModel &_graphModel;

    ConnectionState _connectionState;

    mutable QPointF _out;
    mutable QPointF _in;

    // CICADA perf (P2): per-paint, pointsC1C2() used to recompute the
    // bezier control points from scratch — and it's called from paint(),
    // shape(), AND boundingRect(), so a single redraw of a 100-connection
    // scene was ~300 recomputes. Cache here, invalidate on move() and
    // setEndPoint() (the only inputs to the calculation).
    mutable std::pair<QPointF, QPointF> _cachedC1C2{};
    mutable bool _c1c2Valid{false};
};

} // namespace QtNodes
