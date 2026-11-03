#pragma once

#include "AbstractNodePainter.hpp"
#include "Definitions.hpp"
#include <QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

namespace QtNodes {

class AbstractGraphModel;
class AbstractNodeGeometry;
class BasicGraphicsScene;
class GraphModel;
class NodeGeometry;
class NodeGraphicsObject;
class NodeState;

/// @ Lightweight class incapsulating paint code.
class NODE_EDITOR_PUBLIC DefaultNodePainter : public AbstractNodePainter
{
public:
    void paint(QPainter *painter, NodeGraphicsObject &ngo) const override;

    /// CICADA (ADR-1882): the frame and the colour of the node, flat, with no
    /// pen at all on a node a few pixels across; no port dots, captions, icons
    /// or labels (see NodeDetail.hpp).
    void paintReduced(QPainter *painter, NodeGraphicsObject &ngo, qreal lod) const override;

    void drawNodeRect(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawFilledConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawNodeCaption(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawNodeLabel(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawEntryLabels(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawResizeRect(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawProcessingIndicator(QPainter *painter, NodeGraphicsObject &ngo) const;

    /// \brief The rectangle the node's own icon is drawn in (ADR-1849): the
    ///        bottom-right corner (bottom-left on a resizable node), one
    ///        function for the painter and for whoever has to keep a caption
    ///        out of it.
    /// \param model The graph model.
    /// \param nodeId The node.
    /// \param geometry The node geometry (its size is read).
    /// \param pixmapOut When not null, the icon's picture.
    /// \return The rectangle in node coordinates; empty when the node has no icon.
    static QRect cornerIconRect(AbstractGraphModel &model, NodeId nodeId,
                                AbstractNodeGeometry &geometry,
                                QPixmap *pixmapOut = nullptr);

    void drawValidationIcon(QPainter *painter, NodeGraphicsObject &ngo) const;

    void drawProgressValue(QPainter *painter, NodeGraphicsObject &ngo) const;

private:
    QIcon _toolTipIcon{":/info-tooltip.svg"};
};
} // namespace QtNodes
