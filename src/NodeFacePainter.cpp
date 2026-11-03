// NodeFacePainter.cpp - paints a node's rows, one painter per RowKind
// (CICADA, ADR-1631).
#include "NodeFacePainter.hpp"

#include "rows/RowOps.hpp"

#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>

#include <cstddef>

namespace QtNodes {

void NodeFacePainter::paint(QPainter &painter, const NodeFaceSource &face,
                            const FaceLayoutResult &layout,
                            const FacePaintState &state) {
  const std::shared_ptr<const std::vector<RowSpec>> rows = face.rows();
  if (!rows) {
    return;
  }
  painter.save();
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  // The rows are drawn in the font the layout measured and a legacy widget was
  // drawn in: the application's, not whatever the view's painter carries.
  painter.setFont(QGuiApplication::font());
  for (const RowGeometry &geometry : layout.rows) {
    if ((geometry.index < 0) ||
        (static_cast<std::size_t>(geometry.index) >= rows->size())) {
      continue;
    }
    const RowSpec &spec = (*rows)[static_cast<std::size_t>(geometry.index)];
    const RowPaintArgs args{painter,
                            spec,
                            geometry,
                            face,
                            state,
                            face.rowEnabled(geometry.index),
                            state.hoverRow == geometry.index};
    rowOps(spec.kind).paint(args);
  }
  painter.restore();
}

} // namespace QtNodes
