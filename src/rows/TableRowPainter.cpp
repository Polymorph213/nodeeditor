// TableRowPainter.cpp - the Table row: a read-only preview (CICADA,
// ADR-1631), or a spreadsheet when its face source is a TableFaceSource
// (the Panel, ADR-1804, ADR-1872).
#include "RowOps.hpp"

#include "QtNodes/internal/TableFace.hpp"

#include <QtGui/QFontMetrics>
#include <QtGui/QPen>
#include <QtGui/QPolygon>

#include <algorithm>
#include <vector>

namespace QtNodes {

namespace {

constexpr int kDefaultLines = 3;
constexpr int kTableWidth = 220;
/// Below this scale of the view a spreadsheet shows its summary only.
constexpr double kSummaryScale = 0.5;
/// The padding of a spreadsheet's text in its cell.
constexpr int kPad = 3;

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
  int drawn = 0;
  std::vector<int> branchLines;
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

/// The text of one cell of a spreadsheet, in its look.
void paintSheetText(QPainter &p, const QRect &cell, const QString &text,
                    const TableCellLook &look, const QColor &colour,
                    const QFont &base) {
  QFont font = base;
  font.setBold(look.bold);
  if (look.monospace) {
    font.setFamily(QStringLiteral("Consolas"));
    font.setStyleHint(QFont::Monospace);
  }
  p.setFont(font);
  p.setPen(colour);
  const QFontMetrics fm(font);
  const QRect inner = cell.adjusted(kPad, 0, -kPad, 0);
  p.drawText(inner, Qt::AlignVCenter | static_cast<int>(look.alignment),
             fm.elidedText(text, Qt::ElideRight, std::max(0, inner.width())));
}

/// A branch line: the path in the branch colour in bold, then its count and
/// its Group By label dimmed, across the whole table.
void paintBranchLine(QPainter &p, const QRect &line, const QString &label,
                     const TableCellLook &look, const TableLook &table,
                     const QFont &base) {
  const qsizetype split = label.indexOf(QStringLiteral("  "));
  const QString path = (split < 0) ? label : label.left(split);
  QString rest = (split < 0) ? QString() : label.mid(split);
  if (!look.branchRest.isEmpty()) {
    rest += QStringLiteral("   ") + look.branchRest;
  }
  QFont bold = base;
  bold.setBold(true);
  QRect r = line.adjusted(kPad + 1, 0, -kPad, 0);
  p.setFont(bold);
  p.setPen(look.foreground.isValid() ? look.foreground : table.text);
  p.drawText(r, Qt::AlignVCenter | Qt::AlignLeft, path);
  r.setLeft(r.left() + QFontMetrics(bold).horizontalAdvance(path));
  p.setFont(base);
  p.setPen(table.dimmed);
  p.drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
             QFontMetrics(base).elidedText(rest, Qt::ElideRight,
                                           std::max(0, r.width())));
}

/// The small triangle of a sorted header.
void paintSortMark(QPainter &p, const QRect &cell, int order,
                   const QColor &colour) {
  const int size = 4;
  const int cx = cell.right() - kPad - size;
  const int cy = cell.center().y();
  QPolygon mark;
  if (order > 0) {
    mark << QPoint(cx - size, cy + 2) << QPoint(cx + size, cy + 2)
         << QPoint(cx, cy - 3);
  } else {
    mark << QPoint(cx - size, cy - 2) << QPoint(cx + size, cy - 2)
         << QPoint(cx, cy + 3);
  }
  p.setPen(Qt::NoPen);
  p.setBrush(colour);
  p.drawPolygon(mark);
  p.setBrush(Qt::NoBrush);
}

/// The two toolbar buttons: + and x.
void paintToolbar(QPainter &p, const QRect &strip, const TableLook &look) {
  const int side = strip.height();
  for (int b = 0; b < 2; ++b) {
    const QRect button(strip.left() + (b * side) + 2, strip.top() + 2,
                       side - 4, side - 4);
    p.setPen(QPen(look.grid, 1.0));
    p.setBrush(look.header);
    p.drawRect(button);
    p.setPen(QPen(look.headerText, 1.5));
    const QPoint c = button.center();
    const int arm = std::max(2, (side - 4) / 4);
    if (b == 0) {
      p.drawLine(c.x() - arm, c.y(), c.x() + arm, c.y());
      p.drawLine(c.x(), c.y() - arm, c.x(), c.y() + arm);
    } else {
      p.drawLine(c.x() - arm, c.y() - arm, c.x() + arm, c.y() + arm);
      p.drawLine(c.x() - arm, c.y() + arm, c.x() + arm, c.y() - arm);
    }
  }
  p.setBrush(Qt::NoBrush);
}

/// The resize grip of the legacy panel: three diagonal lines.
void paintGrip(QPainter &p, const QRect &corner) {
  p.setPen(QPen(QColor(120, 120, 120), 1.5));
  const int w = corner.right() - 1;
  const int h = corner.bottom() - 1;
  for (int i = 0; i < 3; ++i) {
    const int offset = (i * 4) + 2;
    p.drawLine(w - offset, h, w, h - offset);
  }
}

/// A spreadsheet: tinted, with a header, styled cells, a strip above and a
/// footer under (the Panel, ADR-1872).
void paintSheet(const RowPaintArgs &args, const TableFaceSource &sheet) {
  const QRect &box = args.geometry.control;
  const int row = args.geometry.index;
  QPainter &p = args.painter;
  const TableLook look = sheet.tableLook(row);
  p.save();
  p.setClipRect(box);
  p.fillRect(box, look.background);
  const QFont base = look.hasFont ? look.font : p.font();
  p.setFont(base);
  if (p.worldTransform().m11() < kSummaryScale) {
    // Far out: the caption and the counts, as the legacy panel showed.
    p.setPen(look.text);
    p.drawText(box.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap,
               sheet.tableSummary(row));
    p.restore();
    return;
  }
  const FaceMetrics metrics = tableMetrics(args.face, row, faceMetrics());
  const TablePieces pieces = tablePieces(args.face, row, box, metrics);
  const TableGeometry g = tableGeometry(args.face, row, pieces.table, metrics);
  QFont bold = base;
  bold.setBold(true);
  if (!pieces.nickname.isEmpty()) {
    p.fillRect(pieces.nickname, look.header);
    p.setFont(bold);
    p.setPen(look.headerText);
    p.drawText(pieces.nickname, Qt::AlignCenter, sheet.tableNickname(row));
  }
  if (!pieces.caption.isEmpty()) {
    p.setFont(base);
    p.setPen(look.dimmed);
    p.drawText(pieces.caption.adjusted(kPad, 0, -kPad, 0),
               Qt::AlignVCenter | Qt::AlignLeft, sheet.tableCaption(row));
  }
  if (!pieces.toolbar.isEmpty()) {
    paintToolbar(p, pieces.toolbar, look);
  }
  // The header.
  const QRect headerBox(pieces.table.left(), pieces.table.top(),
                        pieces.table.width(), g.headerHeight);
  p.fillRect(headerBox, look.header);
  const int sorted = sheet.tableSortColumn(row);
  for (int c = 0; c < g.columns; ++c) {
    const QRect cell = tableCellRect(g, pieces.table, -1, c);
    p.setFont(bold);
    p.setPen(look.headerText);
    const QRect inner = cell.adjusted(kPad, 0, -kPad - 8, 0);
    p.drawText(inner, Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(bold).elidedText(args.face.tableHeader(row, c),
                                             Qt::ElideRight,
                                             std::max(0, inner.width())));
    if (c == sorted) {
      paintSortMark(p, cell, sheet.tableSortOrder(row), look.headerText);
    }
  }
  // The lines.
  const int first = args.face.tableFirstLine(row);
  const int total = args.face.tableLines(row);
  int drawn = 0;
  std::vector<int> branchLines;
  for (int i = 0; i < g.visibleLines; ++i) {
    const int line = first + i;
    if (line >= total) {
      break;
    }
    const QRect lineBox(pieces.table.left(),
                        pieces.table.top() + g.headerHeight +
                            (i * g.lineHeight),
                        pieces.table.width(), g.lineHeight);
    const TableCellLook head = sheet.tableCellLook(row, line, 0);
    drawn = i + 1;
    if (head.branch) {
      branchLines.push_back(i);
      p.fillRect(lineBox, head.background.isValid() ? head.background
                                                    : look.header);
      paintBranchLine(p, lineBox, args.face.tableCell(row, line, 0), head,
                      look, base);
      continue;
    }
    for (int c = 0; c < g.columns; ++c) {
      const QRect cell = tableCellRect(g, pieces.table, i, c);
      const TableCellLook cl =
          (c == 0) ? head : sheet.tableCellLook(row, line, c);
      QColor fill = cl.background.isValid() ? cl.background : look.background;
      QColor ink = cl.foreground.isValid() ? cl.foreground : look.text;
      if (cl.selected) {
        fill = look.selection;
        ink = look.selectionText;
      }
      p.fillRect(cell, fill);
      paintSheetText(p, cell, args.face.tableCell(row, line, c), cl, ink,
                     base);
      if (cl.error) {
        p.setPen(QPen(look.error, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(cell.adjusted(1, 1, -2, -2));
      }
      if (cl.current) {
        p.setPen(QPen(look.selectionText, 1.0, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(cell.adjusted(0, 0, -1, -1));
      }
    }
  }
  // The scroll bar of a long table (the widget's 8 px bar): its track, a
  // thumb as long as the part shown, and the header's end above it.
  if (!pieces.scroll.isEmpty()) {
    p.fillRect(QRect(pieces.scroll.left(), headerBox.top(),
                     pieces.scroll.width(), g.headerHeight),
               look.header);
    p.fillRect(pieces.scroll, look.background.darker(112));
    const QRect thumb =
        tableScrollThumb(pieces.scroll, g.visibleLines, total, first);
    p.setPen(Qt::NoPen);
    p.setBrush(look.grid.lighter(150));
    p.drawRoundedRect(thumb.adjusted(1, 0, -1, 0), 3, 3);
    p.setBrush(Qt::NoBrush);
  }
  // The lines between the cells: in the header and on the lines drawn, and
  // not across a branch line, which spans the table.
  p.setPen(QPen(look.grid, 1.0));
  const int lineTop = pieces.table.top() + g.headerHeight;
  for (int c = 1; c < g.columns; ++c) {
    const int x = pieces.table.left() + g.columnX[static_cast<std::size_t>(c)];
    p.drawLine(x, headerBox.top(), x, headerBox.bottom());
  }
  p.drawLine(pieces.table.left(), headerBox.bottom(), pieces.table.right(),
             headerBox.bottom());
  for (int i = 0; i < drawn; ++i) {
    const int top = lineTop + (i * g.lineHeight);
    const bool span = std::find(branchLines.begin(), branchLines.end(), i) !=
                      branchLines.end();
    if (!span) {
      for (int c = 1; c < g.columns; ++c) {
        const int x =
            pieces.table.left() + g.columnX[static_cast<std::size_t>(c)];
        p.drawLine(x, top, x, top + g.lineHeight - 1);
      }
    }
    p.drawLine(pieces.table.left(), top + g.lineHeight - 1,
               pieces.table.right(), top + g.lineHeight - 1);
  }
  // The footer.
  p.fillRect(pieces.footer, look.background);
  p.setFont(base);
  p.setPen(look.dimmed);
  const QRect foot = pieces.footer.adjusted(kPad + 1, 0, -(kPad + 14), 0);
  p.drawText(foot, Qt::AlignVCenter | Qt::AlignLeft,
             sheet.tableFooterLeft(row));
  const QColor worst = sheet.tableFooterColour(row);
  p.setPen(worst.isValid() ? worst : look.dimmed);
  p.drawText(foot, Qt::AlignVCenter | Qt::AlignRight,
             sheet.tableFooterRight(row));
  paintGrip(p, pieces.footer);
  p.setPen(QPen(look.grid, 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawRect(box.adjusted(0, 0, -1, -1));
  p.restore();
}

void paint(const RowPaintArgs &args) {
  if (const TableFaceSource *sheet = tableFaceOf(args.face)) {
    paintSheet(args, *sheet);
    return;
  }
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
