#pragma once

#include <QPainter>

#include "Export.hpp"

class QPainter;

namespace QtNodes {

class ConnectionGraphicsObject;

/// Class enables custom painting for connections.
class NODE_EDITOR_PUBLIC AbstractConnectionPainter
{
public:
    virtual ~AbstractConnectionPainter() = default;

    /**
     * Reimplement this function in order to have a custom connection painting.
     */
    virtual void paint(QPainter *painter, ConnectionGraphicsObject const &cgo) const = 0;

    /**
     * CICADA (ADR-1882): the wire at a view scale below kReducedDetailBelow
     * (NodeDetail.hpp). A painter that does not reimplement this draws the
     * full wire at every scale.
     * \param painter The painter, in item coordinates.
     * \param cgo The wire.
     * \param lod The level of detail of the item (the view's scale).
     */
    virtual void paintReduced(QPainter *painter, ConnectionGraphicsObject const &cgo, qreal lod) const
    {
        Q_UNUSED(lod);
        paint(painter, cgo);
    }

    virtual QPainterPath getPainterStroke(ConnectionGraphicsObject const &cgo) const = 0;
};
} // namespace QtNodes
