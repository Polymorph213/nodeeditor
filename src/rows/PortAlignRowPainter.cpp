// PortAlignRowPainter.cpp - the PortAlign row: nothing drawn, one port line
// of height, so the rows below it stay on their ports (CICADA, ADR-1631).
#include "RowOps.hpp"

namespace QtNodes {

namespace {

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.portRowHeight;
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
  static_cast<void>(metrics);
  static_cast<void>(spec);
  return 0;
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

void paint(const RowPaintArgs &args) { static_cast<void>(args); }

} // namespace

const RowOps kPortAlignRowOps = {height, labelWidth, controlWidth, false,
                                 place, paint};

} // namespace QtNodes
