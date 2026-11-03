// ButtonRowPainter.cpp - the Button row: a push button (CICADA, ADR-1631).
#include "RowOps.hpp"

#include <QtGui/QPen>

namespace QtNodes {

namespace {

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.buttonHeight;
}

int labelWidth(const TextMetrics &text, const FaceMetrics &metrics,
               const RowSpec &spec) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  return 0;
}

int controlWidth(const TextMetrics &text, const FaceMetrics &metrics,
                 const RowSpec &spec) {
  if (spec.fieldWidth != 0U) {
    return static_cast<int>(spec.fieldWidth); // a button of a fixed width
  }
  constexpr int kPad = 8;
  return text.advance(spec.label) + (2 * metrics.cellPad) + kPad;
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

void paint(const RowPaintArgs &args) {
  constexpr int kHoverLighter = 190;
  constexpr int kRestLighter = 150;
  const QRect face = args.geometry.control.adjusted(0, 0, -1, -1);
  const QColor fill =
      args.state.background.lighter(args.hovered ? kHoverLighter
                                                 : kRestLighter);
  args.painter.setPen(QPen(args.state.background.darker(180), 1.0));
  args.painter.setBrush(fill);
  args.painter.drawRoundedRect(face, 3.0, 3.0);
  paintCellText(args.painter, face, args.spec.label, Qt::AlignCenter,
                faceTextColor(args.enabled));
}

} // namespace

const RowOps kButtonRowOps = {height, labelWidth, controlWidth, false, place,
                              paint};

} // namespace QtNodes
