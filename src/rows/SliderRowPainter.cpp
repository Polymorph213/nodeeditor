// SliderRowPainter.cpp - the Slider row: a groove with a handle (CICADA,
// ADR-1631). Its drag is the item's (S4 converts SliderNode).
#include "RowOps.hpp"

#include <QtGui/QPen>

#include <algorithm>

namespace QtNodes {

namespace {

constexpr int kSliderWidth = 140;

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.comboHeight;
}

int labelWidth(const TextMetrics &text, const FaceMetrics &metrics,
               const RowSpec &spec) {
  return labelCellWidth(text, metrics, spec);
}

int controlWidth(const TextMetrics &text, const FaceMetrics &metrics,
                 const RowSpec &spec) {
  static_cast<void>(text);
  if (spec.fieldWidth != 0U) {
    return static_cast<int>(spec.fieldWidth); // the groove the node measured
  }
  return std::max(metrics.minFieldWidth, kSliderWidth);
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
  const QRect &box = args.geometry.control;
  paintCell(args.painter, box, args.state.background);
  const double span = args.spec.max - args.spec.min;
  double fraction = 0.0;
  if (span > 0.0) {
    fraction =
        (args.face.value(args.geometry.index).toDouble() - args.spec.min) /
        span;
  }
  fraction = std::min(1.0, std::max(0.0, fraction));
  const int grooveY = box.center().y();
  const int left = box.left() + 6;
  const int right = box.right() - 6;
  args.painter.setPen(QPen(args.state.background.darker(180), 3.0));
  args.painter.drawLine(left, grooveY, right, grooveY);
  const int handleX = left + static_cast<int>(fraction * (right - left));
  args.painter.setPen(QPen(faceTextColor(args.enabled), 1.0));
  args.painter.setBrush(args.state.background.lighter(160));
  args.painter.drawRect(
      QRect(handleX - 4, box.top() + 3, 8, box.height() - 6));
}

} // namespace

const RowOps kSliderRowOps = {height, labelWidth, controlWidth, true, place,
                              paint};

} // namespace QtNodes
