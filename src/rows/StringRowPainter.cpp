// StringRowPainter.cpp - the String row: label and a text field (CICADA,
// ADR-1631).
#include "RowOps.hpp"

namespace QtNodes {

namespace {

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.textEditHeight;
}

int labelWidth(const TextMetrics &text, const FaceMetrics &metrics,
               const RowSpec &spec) {
  return labelCellWidth(text, metrics, spec);
}

int controlWidth(const TextMetrics &text, const FaceMetrics &metrics,
                 const RowSpec &spec) {
  if (spec.fieldWidth != 0U) {
    return static_cast<int>(spec.fieldWidth); // a node that sizes its own edit
  }
  // QLineEdit::sizeHint: 17 times the width of an 'x', and the padding.
  constexpr int kChars = 17;
  return (kChars * text.advance(QStringLiteral("x"))) + metrics.cellPad;
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
  const bool editing = (args.state.editRow == args.geometry.index);
  paintFieldCell(args,
                 editing ? QString()
                         : args.face.displayText(args.geometry.index),
                 Qt::AlignLeft);
}

} // namespace

const RowOps kStringRowOps = {height, labelWidth, controlWidth, true, place,
                              paint};

} // namespace QtNodes
