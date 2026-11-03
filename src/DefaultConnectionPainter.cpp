#include "DefaultConnectionPainter.hpp"

#include "AbstractGraphModel.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionState.hpp"
#include "Definitions.hpp"
#include "NodeData.hpp"
#include "StyleCollection.hpp"

#include <QtGui/QIcon>

// CICADA: semantic wire-color palette (red=Number, blue=String, ...,
// white=mixed). Replaces the QtNodes hash-seeded color when the
// CICADA palette returns a valid color; falls back to the default
// hash color otherwise.
#include "cicada/ui/WireColors.h"

namespace QtNodes {

QPainterPath DefaultConnectionPainter::cubicPath(ConnectionGraphicsObject const &connection) const
{
    QPointF const &in = connection.endPoint(PortType::In);
    QPointF const &out = connection.endPoint(PortType::Out);

    auto const c1c2 = connection.pointsC1C2();

    // cubic spline
    QPainterPath cubic(out);

    cubic.cubicTo(c1c2.first, c1c2.second, in);

    return cubic;
}

void DefaultConnectionPainter::drawSketchLine(QPainter *painter,
                                              ConnectionGraphicsObject const &cgo) const
{
    ConnectionState const &state = cgo.connectionState();

    if (state.requiresPort() || state.frozen()) {
        auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

        // CICADA: color the draft wire to match the type of the
        // attached source. The "required port" is the end NOT attached;
        // the opposite end is the source we color against.
        QColor sketchColor = connectionStyle.constructionColor();
        {
            using QtNodes::PortType;
            AbstractGraphModel const &graphModel = cgo.graphModel();
            PortType requiredPort = state.requiredPort();
            auto const cId = cgo.connectionId();
            NodeId attachedNodeId = NodeId{};
            PortIndex attachedPortIdx = 0;
            PortType attachedPortType = PortType::Out;
            if (requiredPort == PortType::In) {
                // Dragging FROM an out port toward an in port.
                attachedNodeId = cId.outNodeId;
                attachedPortIdx = cId.outPortIndex;
                attachedPortType = PortType::Out;
            } else if (requiredPort == PortType::Out) {
                // Dragging FROM an in port toward an out port (rare).
                attachedNodeId = cId.inNodeId;
                attachedPortIdx = cId.inPortIndex;
                attachedPortType = PortType::In;
            }
            if (attachedNodeId != InvalidNodeId) {
                auto dataType = graphModel
                                    .portData(attachedNodeId, attachedPortType,
                                              attachedPortIdx,
                                              PortRole::DataType)
                                    .value<NodeDataType>();
                std::shared_ptr<NodeData> attachedData;
                if (attachedPortType == PortType::Out) {
                    attachedData = graphModel
                                       .portData(attachedNodeId,
                                                 attachedPortType,
                                                 attachedPortIdx,
                                                 PortRole::Data)
                                       .value<std::shared_ptr<NodeData>>();
                }
                QColor cicadaColor =
                    cicada::ui::wireColorFor(dataType, attachedData);
                if (cicadaColor.isValid()) {
                    sketchColor = cicadaColor;
                }
            }
        }

        QPen pen;
        pen.setWidth(static_cast<int>(connectionStyle.constructionLineWidth()));
        pen.setColor(sketchColor);
        pen.setStyle(Qt::DashLine);

        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        auto cubic = cubicPath(cgo);

        // cubic spline
        painter->drawPath(cubic);
    }
}

void DefaultConnectionPainter::drawHoveredOrSelected(QPainter *painter,
                                                     ConnectionGraphicsObject const &cgo) const
{
    bool const hovered = cgo.connectionState().hovered();
    bool const selected = cgo.isSelected();

    // drawn as a fat background
    if (hovered || selected) {
        auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

        double const lineWidth = connectionStyle.lineWidth();

        QPen pen;
        pen.setWidth(static_cast<int>(2 * lineWidth));
        pen.setColor(selected ? connectionStyle.selectedHaloColor()
                              : connectionStyle.hoveredColor());

        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        // cubic spline
        auto const cubic = cubicPath(cgo);
        painter->drawPath(cubic);
    }
}

void DefaultConnectionPainter::drawNormalLine(QPainter *painter,
                                              ConnectionGraphicsObject const &cgo) const
{
    ConnectionState const &state = cgo.connectionState();

    if (state.requiresPort() || state.frozen())
        return;

    // colors

    auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

    QColor normalColorOut = connectionStyle.normalColor();
    QColor normalColorIn = connectionStyle.normalColor();
    QColor selectedColor = connectionStyle.selectedColor();

    bool useGradientColor = false;

    AbstractGraphModel const &graphModel = cgo.graphModel();

    {
        using QtNodes::PortType;

        auto const cId = cgo.connectionId();

        auto dataTypeOut = graphModel
                               .portData(cId.outNodeId,
                                         PortType::Out,
                                         cId.outPortIndex,
                                         PortRole::DataType)
                               .value<NodeDataType>();

        // CICADA: wire color is always the output's. Fetch the source
        // data ptr so wireColorFor() can inspect TypedListData
        // homogeneity at the variant level. PanelNode and other
        // CICADA wires now show in semantic colors instead of QtNodes'
        // hash-seeded random palette.
        auto sourceData = graphModel
                              .portData(cId.outNodeId,
                                        PortType::Out,
                                        cId.outPortIndex,
                                        PortRole::Data)
                              .value<std::shared_ptr<NodeData>>();

        QColor cicadaColor = cicada::ui::wireColorFor(dataTypeOut, sourceData);
        if (cicadaColor.isValid()) {
            // Single color across the whole wire — matches user's
            // intent: "the wire color is always the output."
            useGradientColor = false;
            normalColorOut = cicadaColor;
            normalColorIn = cicadaColor;
            selectedColor = cicadaColor.darker(200);
        } else if (connectionStyle.useDataDefinedColors()) {
            // Fall back to QtNodes' hash-seeded palette only when the
            // CICADA palette can't classify the type.
            auto dataTypeIn = graphModel
                                  .portData(cId.inNodeId,
                                            PortType::In,
                                            cId.inPortIndex,
                                            PortRole::DataType)
                                  .value<NodeDataType>();

            useGradientColor = (dataTypeOut.id != dataTypeIn.id);

            normalColorOut = connectionStyle.normalColor(dataTypeOut.id);
            normalColorIn = connectionStyle.normalColor(dataTypeIn.id);
            selectedColor = normalColorOut.darker(200);
        }
    }

    // geometry

    double const lineWidth = connectionStyle.lineWidth();

    // draw normal line
    QPen p;

    p.setWidth(lineWidth);

    bool const selected = cgo.isSelected();

    auto cubic = cubicPath(cgo);
    if (useGradientColor) {
        painter->setBrush(Qt::NoBrush);

        QColor cOut = normalColorOut;

        if (selected)
            cOut = cOut.darker(200);
        p.setColor(cOut);
        painter->setPen(p);

        unsigned int constexpr segments = 60;

        for (unsigned int i = 0ul; i < segments; ++i) {
            double ratioPrev = double(i) / segments;
            double ratio = double(i + 1) / segments;

            if (i == segments / 2) {
                QColor cIn = normalColorIn;
                if (selected)
                    cIn = cIn.darker(200);

                p.setColor(cIn);
                painter->setPen(p);
            }
            painter->drawLine(cubic.pointAtPercent(ratioPrev), cubic.pointAtPercent(ratio));
        }

        {
            QIcon icon(":convert.png");

            QPixmap pixmap = icon.pixmap(QSize(22, 22));
            painter->drawPixmap(cubic.pointAtPercent(0.50)
                                    - QPoint(pixmap.width() / 2, pixmap.height() / 2),
                                pixmap);
        }
    } else {
        p.setColor(normalColorOut);

        if (selected) {
            p.setColor(selectedColor);
        }

        painter->setPen(p);
        painter->setBrush(Qt::NoBrush);

        painter->drawPath(cubic);
    }
}

void DefaultConnectionPainter::paint(QPainter *painter, ConnectionGraphicsObject const &cgo) const
{
    drawHoveredOrSelected(painter, cgo);

    drawSketchLine(painter, cgo);

    drawNormalLine(painter, cgo);

#ifdef NODE_DEBUG_DRAWING
    debugDrawing(painter, cgo);
#endif

    // draw end points
    auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

    double const pointDiameter = connectionStyle.pointDiameter();

    painter->setPen(connectionStyle.constructionColor());
    painter->setBrush(connectionStyle.constructionColor());
    double const pointRadius = pointDiameter / 2.0;

    // CICADA (ADR-1883): an end dot outside the clip (the item's exposed
    // rectangle, set by ConnectionGraphicsObject::paint) draws nothing but is
    // an antialiased ellipse all the same, and a long wire has both of its
    // ends far from the view: at a view scale of 1 in a document of ten
    // thousand nodes the dots were a fifth of the frame. An empty clip means
    // no clip is known: both are drawn.
    QRectF const clip = painter->clipBoundingRect();
    auto const dotShows = [&clip, pointRadius](QPointF const &centre) {
        double const reach = pointRadius + 1.0; // the pen is one unit wide
        return clip.isEmpty()
               || clip.intersects(QRectF(centre.x() - reach, centre.y() - reach, 2.0 * reach,
                                         2.0 * reach));
    };
    if (dotShows(cgo.out()))
        painter->drawEllipse(cgo.out(), pointRadius, pointRadius);
    if (dotShows(cgo.in()))
        painter->drawEllipse(cgo.in(), pointRadius, pointRadius);
}

void DefaultConnectionPainter::paintReduced(QPainter *painter,
                                            ConnectionGraphicsObject const &cgo,
                                            qreal lod) const
{
    Q_UNUSED(lod);
    // CICADA (ADR-1882): a wire being dragged keeps its full look, the user is
    // working on it; every other wire is a straight one-pixel line from end to
    // end. The curve (a cubic path, stroked and antialiased), the type colour
    // (two port-data lookups) and the end dots are what a frame at full
    // zoom-out paid for fifteen thousand times.
    ConnectionState const &state = cgo.connectionState();
    if (state.requiresPort() || state.frozen()) {
        paint(painter, cgo);
        return;
    }

    auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();
    bool const selected = cgo.isSelected();

    bool const antialiased = painter->testRenderHint(QPainter::Antialiasing);
    if (antialiased)
        painter->setRenderHint(QPainter::Antialiasing, false);

    // Width 0 is one device pixel; the selected wire's halo is two (cosmetic:
    // the width is in device pixels, not in scene units).
    QPen pen(selected ? connectionStyle.selectedHaloColor() : connectionStyle.normalColor(),
             selected ? 2.0 : 0.0);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawLine(cgo.out(), cgo.in());

    if (antialiased)
        painter->setRenderHint(QPainter::Antialiasing, true);
}

QPainterPath DefaultConnectionPainter::getPainterStroke(
    ConnectionGraphicsObject const &connection) const
{
    auto cubic = cubicPath(connection);

    QPointF const &out = connection.endPoint(PortType::Out);
    QPainterPath result(out);

    unsigned int constexpr segments = 20;

    for (auto i = 0ul; i < segments; ++i) {
        double ratio = double(i + 1) / segments;
        result.lineTo(cubic.pointAtPercent(ratio));
    }

    QPainterPathStroker stroker;
    stroker.setWidth(10.0);

    return stroker.createStroke(result);
}

#ifdef NODE_DEBUG_DRAWING
void DefaultConnectionPainter::debugDrawing(QPainter *painter, ConnectionGraphicsObject const &cgo)
{
    Q_UNUSED(painter);

    {
        QPointF const &in = cgo.endPoint(PortType::In);
        QPointF const &out = cgo.endPoint(PortType::Out);

        auto const points = cgo.pointsC1C2();

        painter->setPen(Qt::red);
        painter->setBrush(Qt::red);

        painter->drawLine(QLineF(out, points.first));
        painter->drawLine(QLineF(points.first, points.second));
        painter->drawLine(QLineF(points.second, in));
        painter->drawEllipse(points.first, 3, 3);
        painter->drawEllipse(points.second, 3, 3);

        painter->setBrush(Qt::NoBrush);
        painter->drawPath(cubicPath(cgo));
    }

    {
        painter->setPen(Qt::yellow);
        painter->drawRect(cgo.boundingRect());
    }
}
#endif

} // namespace QtNodes
