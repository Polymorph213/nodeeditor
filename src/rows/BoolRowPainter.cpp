// BoolRowPainter.cpp - the Bool row: a check box with its label (CICADA,
// ADR-1631).
#include "RowOps.hpp"

#include <QtGui/QPen>

namespace QtNodes {

namespace {

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.checkHeight;
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
  if (isInlineRow(spec)) {
    // An inline check box: the indicator, and its text when it has one.
    constexpr int kInlineGap = 6;
    constexpr int kInlinePad = 4;
    return metrics.checkBox + 2 * kInlinePad +
           (spec.label.isEmpty() ? 0
                                 : kInlineGap + text.advance(spec.label));
  }
  // The legacy check box cell: the box, a small gap and the text (calibrated
  // against RotaryTableCalibration).
  constexpr int kGap = 2;
  return metrics.checkBox + kGap + text.advance(spec.label) + metrics.cellPad;
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

void paintInline(const RowPaintArgs &args) {
  const QRect &box = args.geometry.control;
  constexpr int kInset = 2;
  constexpr int kSide = 14;
  const QRect square(box.left() + kInset,
                     box.top() + ((box.height() - kSide) / 2), kSide, kSide);
  args.painter.fillRect(square, faceEditFill(args.enabled));
  args.painter.setPen(QPen(QColor(40, 40, 40), 1.0));
  args.painter.setBrush(Qt::NoBrush);
  args.painter.drawRect(square.adjusted(0, 0, -1, -1));
  if (args.face.value(args.geometry.index).toBool()) {
    args.painter.setPen(QPen(faceEditText(args.enabled), 2.0));
    args.painter.drawLine(square.left() + 3, square.top() + 7,
                          square.left() + 6, square.bottom() - 3);
    args.painter.drawLine(square.left() + 6, square.bottom() - 3,
                          square.right() - 3, square.top() + 3);
  }
  if (!args.spec.label.isEmpty()) {
    paintCellText(args.painter, box.adjusted(kInset + kSide + 6, 0, 0, 0),
                  args.spec.label, Qt::AlignVCenter | Qt::AlignLeft,
                  faceTextColor(args.enabled));
  }
}

void paint(const RowPaintArgs &args) {
  if (isInlineRow(args.spec)) {
    paintInline(args);
    return;
  }
  const QRect &box = args.geometry.control;
  paintCell(args.painter, box, args.state.background);
  constexpr int kInset = 4;
  constexpr int kSide = 14;
  const QRect square(box.left() + kInset,
                     box.top() + ((box.height() - kSide) / 2), kSide, kSide);
  args.painter.setPen(QPen(QColor(150, 150, 150), 1.0));
  args.painter.setBrush(Qt::NoBrush);
  args.painter.drawRect(square);
  if (args.face.value(args.geometry.index).toBool()) {
    args.painter.setPen(QPen(faceTextColor(args.enabled), 2.0));
    args.painter.drawLine(square.left() + 3, square.top() + 7,
                          square.left() + 6, square.bottom() - 3);
    args.painter.drawLine(square.left() + 6, square.bottom() - 3,
                          square.right() - 3, square.top() + 3);
  }
  paintCellText(args.painter, box.adjusted(kInset + kSide + 6, 0, -kInset, 0),
                args.spec.label, Qt::AlignVCenter | Qt::AlignLeft,
                faceTextColor(args.enabled));
}

} // namespace

const RowOps kBoolRowOps = {height, labelWidth, controlWidth, false, place,
                            paint};

} // namespace QtNodes
