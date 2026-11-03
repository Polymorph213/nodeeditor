// NodeFacePainter.cpp - paints a node's rows, one painter per RowKind
// (CICADA, ADR-1631).
#include "NodeFacePainter.hpp"

#include "rows/RowOps.hpp"

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>
#include <QtGui/QPen>
#include <QtGui/QPainter>

#include <cstddef>

namespace QtNodes {

namespace {

/// A legacy group box or heading (ADR-1851): the frame is drawn from half a
/// title's height below the rectangle's top, the title on its edge on the
/// body's colour; a heading is bold text in its rectangle.
void paintFrame(QPainter &painter, const FaceFrame &frame,
                const QColor &background) {
  const QFont plain = painter.font();
  if (frame.heading) {
    QFont bold = plain;
    bold.setBold(true);
    painter.setFont(bold);
    painter.setPen(faceTextColor(true));
    painter.drawText(frame.rect, Qt::AlignVCenter | Qt::AlignLeft,
                     frame.title);
    painter.setFont(plain);
    return;
  }
  const QFontMetrics metrics(plain);
  const int lift = frame.title.isEmpty() ? 0 : (metrics.height() / 2);
  const QRect box(frame.rect.left(), frame.rect.top() + lift,
                  frame.rect.width() - 1, frame.rect.height() - 1 - lift);
  painter.setPen(QPen(QColor(112, 112, 112), 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(box, 3.0, 3.0);
  if (!frame.title.isEmpty()) {
    constexpr int kInset = 9;
    constexpr int kPad = 3;
    const QRect label(frame.rect.left() + kInset, frame.rect.top(),
                      metrics.horizontalAdvance(frame.title) + (2 * kPad),
                      metrics.height());
    painter.fillRect(label, background);
    painter.setPen(faceTextColor(true));
    painter.drawText(label, Qt::AlignCenter, frame.title);
  }
}

} // namespace

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
  // The legacy widget's group boxes go behind the rows they hold.
  for (const FaceFrame &frame : face.frames()) {
    paintFrame(painter, frame, state.background);
  }
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
