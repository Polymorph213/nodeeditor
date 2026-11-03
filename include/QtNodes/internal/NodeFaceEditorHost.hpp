// NodeFaceEditorHost.hpp - who edits the rows of a painted node (CICADA,
// ADR-1632).
//
// A painted node has no control to take a press. The node's item finds the
// row under the pointer and hands the press to the scene's host, which opens
// the one editor overlay, steps a number, toggles a box or pops a list. The
// fork knows nothing of how: the application installs the host.
#pragma once

#include "Export.hpp"
#include "FaceLayout.hpp"

#include <QtCore/QPointF>

class QGraphicsSceneMouseEvent;
class QGraphicsSceneWheelEvent;

namespace QtNodes {

class NodeGraphicsObject;

/// \brief What the item of a painted node asks of the editor.
class NODE_EDITOR_PUBLIC NodeFaceEditorHost {
public:
  virtual ~NodeFaceEditorHost();

  /// \brief A press landed on a control of a node's face.
  /// \param item The node's item.
  /// \param hit The row and the part.
  /// \param event The press.
  /// \return true when the host took it: the item then keeps the mouse grab
  ///         and sends the moves and the release to the host.
  virtual bool facePress(NodeGraphicsObject &item, const FaceHit &hit,
                         QGraphicsSceneMouseEvent &event) = 0;

  /// \brief The pointer moved with a taken press.
  /// \param item The node's item.
  /// \param hit The row and the part the press began on.
  /// \param event The move.
  virtual void faceMove(NodeGraphicsObject &item, const FaceHit &hit,
                        QGraphicsSceneMouseEvent &event) = 0;

  /// \brief The button of a taken press was released, or the grab was lost.
  /// \param item The node's item.
  /// \param hit The row and the part the press began on.
  virtual void faceRelease(NodeGraphicsObject &item, const FaceHit &hit) = 0;

  /// \brief A double click on a control (a table opens its editor).
  /// \param item The node's item.
  /// \param hit The row and the part.
  /// \return true when the host took it.
  virtual bool faceDoubleClick(NodeGraphicsObject &item, const FaceHit &hit) = 0;

  /// \brief The wheel turned over a number row.
  /// \param item The node's item.
  /// \param hit The row and the part.
  /// \param steps Notches; positive is up.
  /// \return true when the host took it.
  virtual bool faceWheel(NodeGraphicsObject &item, const FaceHit &hit,
                         int steps) = 0;

  /// \brief A node is being deleted or its item destroyed: an editor, a
  ///        gesture or a pop-up on it ends.
  /// \param node The node.
  virtual void faceNodeGone(NodeId node) = 0;
};

} // namespace QtNodes
