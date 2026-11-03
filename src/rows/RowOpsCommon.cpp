// RowOpsCommon.cpp - the table of row operations and what every row painter
// shares (CICADA, ADR-1631).
#include "RowOps.hpp"

#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <algorithm>

namespace QtNodes {

const RowOps &rowOps(RowKind kind) {
  switch (kind) {
  case RowKind::Label:
    return kLabelRowOps;
  case RowKind::Bool:
    return kBoolRowOps;
  case RowKind::Int:
    return kIntRowOps;
  case RowKind::Double:
    return kDoubleRowOps;
  case RowKind::Enum:
    return kEnumRowOps;
  case RowKind::String:
    return kStringRowOps;
  case RowKind::Vec3:
    return kVec3RowOps;
  case RowKind::Button:
    return kButtonRowOps;
  case RowKind::Section:
    return kSectionRowOps;
  case RowKind::Slider:
    return kSliderRowOps;
  case RowKind::Swatch:
    return kSwatchRowOps;
  case RowKind::Table:
    return kTableRowOps;
  case RowKind::PortAlign:
    return kPortAlignRowOps;
  default:
    return kLabelRowOps;
  }
}

int labelCellWidth(const TextMetrics &text, const FaceMetrics &metrics,
                   const RowSpec &spec) {
  // A row with no label has no label cell (the port beside it names it).
  if (spec.label.isEmpty()) {
    return 0;
  }
  return text.advance(spec.label + QLatin1Char(':')) + metrics.cellPad;
}

int numberFieldWidth(const TextMetrics &text, const FaceMetrics &metrics,
                     const RowSpec &spec, bool integer) {
  // The sample texts of the two range ends, as the box formats them: a
  // bounded real with four decimals, an unbounded one in the short %g form
  // (ADR-1251), a whole number as it is.
  const bool unbounded = ((spec.flags & kRowUnbounded) != 0U);
  const auto sample = [integer, unbounded](double value) {
    if (integer) {
      return QString::number(static_cast<long long>(value));
    }
    return unbounded ? QString::number(value, 'g', 12)
                     : QString::number(value, 'f', 4);
  };
  // QAbstractSpinBox::sizeHint measures each end's text with the suffix and a
  // blank, then adds the frame, the padding and the arrow box. Calibrated
  // against the legacy nodes (AdditiveProcessTemplate, MeshRepair): the
  // overhead of a spin box under the node's style sheet is 45 px.
  constexpr int kSpinOverhead = 45;
  const QString tail = spec.units.isEmpty()
                           ? QString()
                           : (QLatin1Char(' ') + spec.units + QLatin1Char(' '));
  int widest = text.advance(sample(spec.min) + tail);
  widest = std::max(widest, text.advance(sample(spec.max) + tail));
  static_cast<void>(metrics.arrowWidth);
  return std::max(metrics.minFieldWidth, widest + kSpinOverhead);
}

void paintCellText(QPainter &painter, const QRect &cell, const QString &text,
                   int flags, const QColor &color) {
  painter.setPen(color);
  painter.drawText(cell, flags, text);
}

void paintCell(QPainter &painter, const QRect &cell, const QColor &fill) {
  painter.fillRect(cell, fill);
}

void paintStepArrows(QPainter &painter, const QRect &up, const QRect &down,
                     const QColor &edge, bool enabled) {
  const QColor line = enabled ? edge : edge.darker(130);
  painter.setPen(QPen(line, 1.0));
  painter.setBrush(Qt::NoBrush);
  if (!up.isEmpty()) {
    painter.drawRect(up.adjusted(1, 1, -2, 0));
  }
  if (!down.isEmpty()) {
    painter.drawRect(down.adjusted(1, 0, -2, -2));
  }
}

void paintRowLabel(const RowPaintArgs &args) {
  if (args.geometry.label.isEmpty() || args.spec.label.isEmpty()) {
    return;
  }
  paintCell(args.painter, args.geometry.label, args.state.background);
  paintCellText(args.painter, args.geometry.label,
                args.spec.label + QLatin1Char(':'),
                Qt::AlignVCenter | Qt::AlignHCenter,
                faceTextColor(args.enabled));
}

QRect inlineFieldRect(const FaceMetrics &metrics, const QRect &cell) {
  const int editHeight = metrics.portRowHeight - 8;
  const int top = cell.top() + ((cell.height() - editHeight) / 2);
  return QRect(cell.left(), top, cell.width(), editHeight);
}

void paintInlineField(const RowPaintArgs &args, const QString &text,
                      int align) {
  const QRect &box = args.geometry.control;
  args.painter.fillRect(box, faceEditFill(args.enabled));
  args.painter.setPen(QPen(QColor(40, 40, 40), 1.0));
  args.painter.setBrush(Qt::NoBrush);
  args.painter.drawRect(box.adjusted(0, 0, -1, -1));
  paintCellText(args.painter, box.adjusted(3, 0, -3, 0), text,
                align | Qt::AlignVCenter, faceEditText(args.enabled));
}

void paintFieldCell(const RowPaintArgs &args, const QString &text, int align) {
  const QRect &box = args.geometry.control;
  paintCell(args.painter, box, args.state.background);
  args.painter.setPen(QPen(args.state.background.darker(140), 1.0));
  args.painter.setBrush(Qt::NoBrush);
  args.painter.drawRect(box.adjusted(0, 0, -1, -1));
  QRect inner = box.adjusted(6, 0, -6, 0);
  if (!args.geometry.stepUp.isEmpty()) {
    inner.setRight(args.geometry.stepUp.left() - 2);
  }
  paintCellText(args.painter, inner, text, align | Qt::AlignVCenter,
                faceTextColor(args.enabled));
}

} // namespace QtNodes
