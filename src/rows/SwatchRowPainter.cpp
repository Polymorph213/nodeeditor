// SwatchRowPainter.cpp - the Swatch row: a colour square (CICADA, ADR-1631).
// Its picker is the application's (S4 converts ColorSwatchNode).
#include "RowOps.hpp"

#include <QtGui/QPen>

namespace QtNodes {

namespace {

constexpr int kSwatchWidth = 60;

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.textEditHeight;
}

int labelWidth(const TextMetrics &text, const FaceMetrics &metrics,
               const RowSpec &spec) {
  return spec.label.isEmpty() ? 0 : labelCellWidth(text, metrics, spec);
}

int controlWidth(const TextMetrics &text, const FaceMetrics &metrics,
                 const RowSpec &spec) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  return kSwatchWidth;
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

void paint(const RowPaintArgs &args) {
  paintRowLabel(args);
  constexpr int kUnsetGrey = 128;
  const QColor colour(args.face.value(args.geometry.index).toString());
  args.painter.setPen(QPen(QColor(40, 40, 40), 1.0));
  args.painter.setBrush(colour.isValid()
                            ? colour
                            : QColor(kUnsetGrey, kUnsetGrey, kUnsetGrey));
  args.painter.drawRect(args.geometry.control.adjusted(0, 0, -1, -1));
}

} // namespace

const RowOps kSwatchRowOps = {height, labelWidth, controlWidth, true, place,
                              paint};

} // namespace QtNodes
