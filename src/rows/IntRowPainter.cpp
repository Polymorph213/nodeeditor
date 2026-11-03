// IntRowPainter.cpp - the Int row: label, field and step arrows (CICADA,
// ADR-1631).
#include "RowOps.hpp"

namespace QtNodes {

namespace {

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.spinHeight;
}

int labelWidth(const TextMetrics &text, const FaceMetrics &metrics,
               const RowSpec &spec) {
  return labelCellWidth(text, metrics, spec);
}

int controlWidth(const TextMetrics &text, const FaceMetrics &metrics,
                 const RowSpec &spec) {
  if (isInlineRow(spec)) {
    return inlineFieldWidth(spec, metrics.inlineIntWidth);
  }
  return numberFieldWidth(text, metrics, spec, true);
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  if (isInlineRow(spec)) {
    geometry.control = inlineFieldRect(metrics, geometry.control);
    return; // a plain edit: no arrows
  }
  const QRect &box = geometry.control;
  const int width = metrics.arrowWidth;
  const int half = box.height() / 2;
  geometry.stepUp = QRect(box.right() - width + 1, box.top(), width, half);
  geometry.stepDown = QRect(box.right() - width + 1, box.top() + half, width,
                            box.height() - half);
}

void paint(const RowPaintArgs &args) {
  paintRowLabel(args);
  const bool editing = (args.state.editRow == args.geometry.index);
  if (isInlineRow(args.spec)) {
    paintInlineField(args,
                     editing ? QString()
                             : args.face.displayText(args.geometry.index),
                     Qt::AlignCenter);
    return;
  }
  paintFieldCell(args,
                 editing ? QString()
                         : args.face.displayText(args.geometry.index),
                 Qt::AlignLeft);
  paintStepArrows(args.painter, args.geometry.stepUp, args.geometry.stepDown,
                  args.state.background.darker(160), args.enabled);
}

} // namespace

const RowOps kIntRowOps = {height, labelWidth, controlWidth, true, place,
                           paint};

} // namespace QtNodes
