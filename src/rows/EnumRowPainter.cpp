// EnumRowPainter.cpp - the Enum row: label and a field that opens a list
// (CICADA, ADR-1631).
#include "RowOps.hpp"

#include <QtGui/QPainterPath>

#include <algorithm>

namespace QtNodes {

namespace {

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
  if (isInlineRow(spec)) {
    return inlineFieldWidth(spec, metrics.inlineEnumWidth);
  }
  int widest = 0;
  for (const QString &choice : spec.choiceLabels) {
    widest = std::max(widest, text.advance(choice));
  }
  return std::max(metrics.minFieldWidth,
                  widest + metrics.arrowWidth + metrics.cellPad);
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  if (isInlineRow(spec)) {
    geometry.control = inlineFieldRect(metrics, geometry.control);
  }
}

void paintInlineEnum(const RowPaintArgs &args) {
  const QRect &box = args.geometry.control;
  const QRect arrow(box.right() - 14, box.top(), 14, box.height());
  paintInlineField(args, QString(), Qt::AlignLeft);
  paintCellText(args.painter, box.adjusted(4, 0, -(arrow.width() + 2), 0),
                args.face.displayText(args.geometry.index),
                Qt::AlignVCenter | Qt::AlignLeft, faceEditText(args.enabled));
  QPainterPath triangle;
  const QPointF centre = arrow.center();
  triangle.moveTo(centre.x() - 3.5, centre.y() - 1.5);
  triangle.lineTo(centre.x() + 3.5, centre.y() - 1.5);
  triangle.lineTo(centre.x(), centre.y() + 2.5);
  triangle.closeSubpath();
  args.painter.setPen(Qt::NoPen);
  args.painter.setBrush(faceEditText(args.enabled));
  args.painter.drawPath(triangle);
}

void paint(const RowPaintArgs &args) {
  if (isInlineRow(args.spec)) {
    paintInlineEnum(args);
    return;
  }
  paintRowLabel(args);
  const QRect &box = args.geometry.control;
  paintCell(args.painter, box, args.state.background);
  const QRect arrow(box.right() - 16, box.top() + 2, 15, box.height() - 4);
  paintCellText(args.painter, box.adjusted(6, 0, -(arrow.width() + 4), 0),
                args.face.displayText(args.geometry.index),
                Qt::AlignVCenter | Qt::AlignLeft, faceTextColor(args.enabled));
  args.painter.setPen(args.state.background.darker(160));
  args.painter.setBrush(args.state.background.darker(120));
  args.painter.drawRect(arrow);
  QPainterPath triangle;
  const QPointF centre = arrow.center();
  triangle.moveTo(centre.x() - 4.0, centre.y() - 2.0);
  triangle.lineTo(centre.x() + 4.0, centre.y() - 2.0);
  triangle.lineTo(centre.x(), centre.y() + 3.0);
  triangle.closeSubpath();
  args.painter.setPen(Qt::NoPen);
  args.painter.setBrush(faceTextColor(args.enabled));
  args.painter.drawPath(triangle);
}

} // namespace

const RowOps kEnumRowOps = {height, labelWidth, controlWidth, true, place,
                            paint};

} // namespace QtNodes
