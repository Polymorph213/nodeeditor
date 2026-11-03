// LabelRowPainter.cpp - the Label row: one line of text (CICADA, ADR-1631).
#include "RowOps.hpp"

#include <algorithm>

namespace QtNodes {

namespace {

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.labelHeight;
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
  return std::min(text.advance(spec.label) + metrics.cellPad,
                  metrics.maxLabelWidth);
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

void paint(const RowPaintArgs &args) {
  const QString text = args.face.displayText(args.geometry.index);
  paintCellText(args.painter, args.geometry.control.adjusted(4, 0, -4, 0),
                text.isEmpty() ? args.spec.label : text,
                Qt::AlignVCenter | Qt::AlignLeft, faceTextColor(args.enabled));
}

} // namespace

const RowOps kLabelRowOps = {height, labelWidth, controlWidth, false, place,
                             paint};

} // namespace QtNodes
