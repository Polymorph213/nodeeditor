// Vec3RowPainter.cpp - the Vec3 row: X, Y and Z in one port line (CICADA,
// ADR-1631). Drawn as the legacy inline editors of CircleInlineEditors.h.
#include "RowOps.hpp"

#include <QtGui/QPen>

namespace QtNodes {

namespace {

constexpr const char *kAxis[3] = {"X", "Y", "Z"};

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
  static_cast<void>(spec);
  int total = 0;
  for (const char *axis : kAxis) {
    total += text.advance(QString::fromLatin1(axis)) + metrics.vecLabelPad +
             metrics.vecEditWidth;
  }
  return total + (5 * metrics.vecSpacing);
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(spec);
  const int editHeight = metrics.portRowHeight - 8;
  int x = geometry.control.left();
  const int top =
      geometry.control.top() + ((geometry.control.height() - editHeight) / 2);
  for (int i = 0; i < 3; ++i) {
    const int letter =
        text.advance(QString::fromLatin1(kAxis[i])) + metrics.vecLabelPad;
    geometry.axis[i] =
        QRect(x, geometry.control.top(), letter, geometry.control.height());
    x += letter + metrics.vecSpacing;
    geometry.comp[i] = QRect(x, top, metrics.vecEditWidth, editHeight);
    x += metrics.vecEditWidth + metrics.vecSpacing;
  }
}

void paint(const RowPaintArgs &args) {
  for (int i = 0; i < 3; ++i) {
    paintCellText(args.painter, args.geometry.axis[i],
                  QString::fromLatin1(kAxis[i]),
                  Qt::AlignVCenter | Qt::AlignRight, faceTextColor(true));
    const bool editing = (args.state.editRow == args.geometry.index) &&
                         (args.state.editComponent == i);
    args.painter.fillRect(args.geometry.comp[i], faceEditFill(args.enabled));
    args.painter.setPen(QPen(QColor(40, 40, 40), 1.0));
    args.painter.setBrush(Qt::NoBrush);
    args.painter.drawRect(args.geometry.comp[i].adjusted(0, 0, -1, -1));
    if (!editing) {
      paintCellText(args.painter, args.geometry.comp[i],
                    args.face.displayText(args.geometry.index, i),
                    Qt::AlignCenter, faceEditText(args.enabled));
    }
  }
}

} // namespace

const RowOps kVec3RowOps = {height, labelWidth, controlWidth, false, place,
                            paint};

} // namespace QtNodes
