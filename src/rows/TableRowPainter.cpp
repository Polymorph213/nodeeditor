// TableRowPainter.cpp - the Table row: a read-only preview (CICADA,
// ADR-1631). A double click opens the overlay's table (S4 converts the Panel
// and the Structure Transform).
#include "RowOps.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QPen>

#include <algorithm>

namespace QtNodes {

namespace {

constexpr int kDefaultLines = 3;
constexpr int kTableWidth = 220;

/// Lines of the preview: RowSpec::step, three when it is not set.
int lines(const RowSpec &spec) {
  return (spec.step >= 1.0) ? static_cast<int>(spec.step) : kDefaultLines;
}

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  if ((spec.flags & kRowSized) != 0U) {
    return static_cast<int>(spec.max); // the face's own size (the Panel)
  }
  return (lines(spec) + 1) * metrics.labelHeight;
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
  if ((spec.flags & kRowSized) != 0U) {
    return static_cast<int>(spec.min);
  }
  return std::max(metrics.minFieldWidth, kTableWidth);
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  static_cast<void>(geometry);
}

/// The table of a Table row that has one of its own: a header, the lines from
/// the first one shown, and a line between the columns.
void paintTable(const RowPaintArgs &args, const TableGeometry &g) {
  const QRect &box = args.geometry.control;
  const int row = args.geometry.index;
  QPainter &p = args.painter;
  const QFontMetrics fm(p.font());
  p.save();
  p.setClipRect(box);
  paintCell(p, box, args.state.background.darker(115));
  const QColor text = faceTextColor(true);
  const QColor grid = args.state.background.darker(170);
  // The header.
  paintCell(p, QRect(box.left(), box.top(), box.width(), g.headerHeight),
            args.state.background.darker(145));
  for (int c = 0; c < g.columns; ++c) {
    const QRect cell = tableCellRect(g, box, -1, c);
    p.setPen(text);
    p.drawText(cell.adjusted(3, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft,
               fm.elidedText(args.face.tableHeader(row, c), Qt::ElideRight,
                             std::max(0, cell.width() - 6)));
  }
  // The lines.
  const int first = args.face.tableFirstLine(row);
  const int total = args.face.tableLines(row);
  for (int i = 0; i < g.visibleLines; ++i) {
    const int line = first + i;
    if (line >= total) {
      break;
    }
    for (int c = 0; c < g.columns; ++c) {
      const QRect cell = tableCellRect(g, box, i, c);
      p.setPen(text);
      p.drawText(cell.adjusted(3, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft,
                 fm.elidedText(args.face.tableCell(row, line, c),
                               Qt::ElideRight, std::max(0, cell.width() - 6)));
    }
  }
  // The lines between the columns and under the header.
  p.setPen(QPen(grid, 1.0));
  for (int c = 1; c < g.columns; ++c) {
    const int x = box.left() + g.columnX[static_cast<std::size_t>(c)];
    p.drawLine(x, box.top(), x, box.bottom());
  }
  p.drawLine(box.left(), box.top() + g.headerHeight, box.right(),
             box.top() + g.headerHeight);
  p.setBrush(Qt::NoBrush);
  p.drawRect(box.adjusted(0, 0, -1, -1));
  p.restore();
}

void paint(const RowPaintArgs &args) {
  const QRect &box = args.geometry.control;
  const TableGeometry geometry =
      tableGeometry(args.face, args.geometry.index, box, faceMetrics());
  if (geometry.columns > 0) {
    paintTable(args, geometry);
    return;
  }
  paintCell(args.painter, box, args.state.background.darker(115));
  args.painter.setPen(QPen(args.state.background.darker(170), 1.0));
  args.painter.setBrush(Qt::NoBrush);
  args.painter.drawRect(box.adjusted(0, 0, -1, -1));
  paintCellText(args.painter, box.adjusted(4, 2, -4, -2),
                args.face.displayText(args.geometry.index),
                Qt::AlignTop | Qt::AlignLeft, faceTextColor(args.enabled));
}

} // namespace

const RowOps kTableRowOps = {height, labelWidth, controlWidth, false, place,
                             paint};

} // namespace QtNodes
