// NodeFacePainter.hpp - paints the rows of a node's face (CICADA, ADR-1631).
#pragma once

#include "Export.hpp"
#include "FaceLayout.hpp"
#include "NodeFace.hpp"

#include <QtGui/QColor>

class QPainter;

namespace QtNodes {

/// \brief What the painter needs besides the rows: the look of the moment.
struct FacePaintState {
  /// The node's state colour (normal, hidden, error ...): the flat colour the
  /// legacy nodes gave their widgets with a style sheet.
  QColor background = QColor(60, 60, 60);
  /// The row the pointer is over; -1 for none.
  int hoverRow = -1;
  /// The part of it.
  FacePart hoverPart = FacePart::None;
  /// The row an editor is open on; -1 for none. Its text is not painted: the
  /// editor shows it.
  int editRow = -1;
  /// The Vec3 component an editor is open on; -1 for none.
  int editComponent = -1;
};

/// \brief Draws a face with one painter per RowKind.
class NODE_EDITOR_PUBLIC NodeFacePainter {
public:
  /// \brief Paint the face. The painter's origin is the body's top-left.
  /// \param painter The painter, in body coordinates.
  /// \param face The rows and their values.
  /// \param layout Where each row is.
  /// \param state The look of the moment.
  static void paint(QPainter &painter, const NodeFaceSource &face,
                    const FaceLayoutResult &layout,
                    const FacePaintState &state);
};

} // namespace QtNodes
