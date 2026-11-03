#pragma once

#include <QPainter>

#include "Export.hpp"

class QPainter;

namespace QtNodes {

class NodeGraphicsObject;
class NodeDataModel;

/// Class enables custom painting.
class NODE_EDITOR_PUBLIC AbstractNodePainter
{
public:
    virtual ~AbstractNodePainter() = default;

    /**
   * Reimplement this function in order to have a custom painting.
   *
   * Useful functions:
   * `NodeGraphicsObject::nodeScene()->nodeGeometry()`
   * `NodeGraphicsObject::graphModel()`
   */
    virtual void paint(QPainter *painter, NodeGraphicsObject &ngo) const = 0;

    /**
   * CICADA (ADR-1882): the node at a view scale below kReducedDetailBelow
   * (NodeDetail.hpp): its frame and colour, no captions, rows, icons or
   * port labels. A painter that does not reimplement this draws the full node
   * at every scale.
   * \param painter The painter, in item coordinates.
   * \param ngo The node.
   * \param lod The level of detail of the item (the view's scale).
   */
    virtual void paintReduced(QPainter *painter, NodeGraphicsObject &ngo, qreal lod) const
    {
        Q_UNUSED(lod);
        paint(painter, ngo);
    }
};
} // namespace QtNodes
