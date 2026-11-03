// TableRowPainter.cpp - the Table row: a read-only preview (CICADA,
// ADR-1631). A double click opens the overlay's table (S4 converts the Panel
// and the Structure Transform).
#include "RowOps.hpp"

#include <QtGui/QPen>

#include <algorithm>

namespace QtNodes {

namespace {

constexpr int kDefaultLines = 3;
constexpr int kTableWidth = 220;

/// Lines of the preview: RowSpec::step, three when it is not set.
int lines(const RowSpec &spec) {
  return (spec.step >= 1.0) ? static_cast<int>(spec.step) : kDefaultLines;
}

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  return (lines(spec) + 1) * metrics.labelHeight;
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
  static_cast<void>(text);
  static_cast<void>(spec);
  return std::max(metrics.minFieldWidth, kTableWidth);
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

void paint(const RowPaintArgs &args) {
  const QRect &box = args.geometry.control;
  paintCell(args.painter, box, args.state.background.darker(115));
  args.painter.setPen(QPen(args.state.background.darker(170), 1.0));
  args.painter.setBrush(Qt::NoBrush);
  args.painter.drawRect(box.adjusted(0, 0, -1, -1));
  paintCellText(args.painter, box.adjusted(4, 2, -4, -2),
                args.face.displayText(args.geometry.index),
                Qt::AlignTop | Qt::AlignLeft, faceTextColor(args.enabled));
}

} // namespace

const RowOps kTableRowOps = {height, labelWidth, controlWidth, false, place,
                             paint};

} // namespace QtNodes
