// FaceLayout.cpp - the layout of a node's face (CICADA, ADR-1631).
#include "FaceLayout.hpp"

#include "rows/RowOps.hpp"

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace QtNodes {

namespace {

/// A font that does not exist: 7 px a character, a 16 px line.
class NominalTextMetrics final : public TextMetrics {
public:
  int advance(const QString &text, bool bold) const override {
    constexpr int kAdvance = 7;
    constexpr int kBoldExtra = 1;
    return (static_cast<int>(text.size()) * kAdvance) +
           (bold ? static_cast<int>(text.size()) * kBoldExtra : 0);
  }
  int lineHeight() const override { return 16; }
  bool nominal() const override { return true; }
};

/// The application's font.
class QtTextMetrics final : public TextMetrics {
public:
  int advance(const QString &text, bool bold) const override {
    QFont font = QGuiApplication::font();
    font.setBold(bold);
    return QFontMetrics(font).horizontalAdvance(text);
  }
  int lineHeight() const override {
    return QFontMetrics(QGuiApplication::font()).height();
  }
};

} // namespace

TextMetrics::~TextMetrics() = default;

const TextMetrics &TextMetrics::application() {
  static const QtTextMetrics kReal;
  static const NominalTextMetrics kNominal;
  if (QGuiApplication::instance() != nullptr) {
    return kReal;
  }
  return kNominal;
}

FaceMetrics FaceMetrics::forFont(const TextMetrics &text) {
  FaceMetrics metrics;
  const int line = text.lineHeight();
  metrics.textEditHeight = line + 6;
  metrics.spinHeight = line + 9;
  metrics.comboHeight = line + 6;
  metrics.checkHeight = line + 4;
  metrics.buttonHeight = line + 8;
  metrics.sectionHeight = line + 6;
  metrics.labelHeight = line + 4;
  metrics.portRowHeight = line + 10;
  return metrics;
}

const FaceMetrics &faceMetrics() {
  static const FaceMetrics kMetrics =
      FaceMetrics::forFont(TextMetrics::application());
  return kMetrics;
}

namespace {

/// One visible row, measured.
struct Measured {
  int index = 0;
  const RowOps *ops = nullptr;
  int height = 0;
  int labelWidth = 0;
  int controlWidth = 0;
  /// The width of the row's cell: the node's (RowSpec::cellWidth), else the
  /// label and the control.
  int cell = 0;
  int indent = 0;
  /// The node's insets of the row (RowSpec::insetLeft, insetRight).
  int insetLeft = 0;
  int insetRight = 0;
  bool twoColumn = false;
  bool natural = false;
  /// The row asked to sit on the line of the row before it (kRowJoined).
  bool joined = false;
  std::size_t section = 0;
};

/// The rows that share one line: `count` of them from `first`, in `shown`.
struct Line {
  std::size_t first = 0;
  std::size_t count = 1;
  /// The height of the highest row.
  int height = 0;
};

/// A row that keeps its own width and is not stretched to the body's.
bool keepsNaturalWidth(const RowSpec &spec) {
  return (spec.kind == RowKind::Vec3) || (spec.kind == RowKind::PortAlign) ||
         ((spec.flags & kRowInline) != 0U);
}

/// The width of a line's rows side by side, before the share of what is left.
int naturalLineWidth(const std::vector<Measured> &shown, const Line &line,
                     int gap) {
  int width = gap * static_cast<int>(line.count - 1U);
  for (std::size_t k = 0; k < line.count; ++k) {
    const Measured &m = shown[line.first + k];
    width += m.cell;
  }
  return width;
}

/// The height of a row: a port step on a port-aligned face, else the row's
/// own, else the kind's.
int rowHeightOf(const RowSpec &spec, const RowOps &ops,
                const FaceMetrics &metrics, bool compact) {
  if (compact) {
    return metrics.portRowHeight;
  }
  return (spec.rowHeight != 0U) ? static_cast<int>(spec.rowHeight)
                                : ops.height(metrics, spec);
}

} // namespace

FaceLayoutResult layoutFace(const NodeFaceSource &face,
                            const FaceMetrics &metrics,
                            const TextMetrics &text) {
  FaceLayoutResult out;
  out.revision = face.revision();
  out.toggles = face.sectionToggles();
  out.metrics = &metrics;
  out.nominal = text.nominal();
  // A face with no row is as big as its legacy widget was, else nothing
  // (ADR-1832): never the -1 x -1 of a default QSize, which a node's height
  // would add as an unsigned number.
  const QSize least = face.minBodySize();
  out.size = QSize(std::max(0, least.width()), std::max(0, least.height()));

  const std::shared_ptr<const std::vector<RowSpec>> rowsPtr = face.rows();
  if (!rowsPtr) {
    return out;
  }
  const std::vector<RowSpec> &specs = *rowsPtr;
  // The rows of a port-aligned face (the Point, the Circle family) fill the
  // body with no margin and no gap: each sits on a port line.
  const bool compact = face.alignsToPorts();

  // Phase 1: measure every row that is shown.
  std::vector<Measured> shown;
  shown.reserve(specs.size());
  for (std::size_t i = 0; i < specs.size(); ++i) {
    const RowSpec &spec = specs[i];
    const int index = static_cast<int>(i);
    if (!face.rowVisible(index)) {
      continue;
    }
    const bool inSection = (spec.kind != RowKind::Section) &&
                           (spec.section != 0U);
    if (inSection && !face.sectionExpanded(spec.section)) {
      continue;
    }
    Measured m;
    m.index = index;
    m.ops = &rowOps(spec.kind);
    m.height = rowHeightOf(spec, *m.ops, metrics, compact);
    m.labelWidth = m.ops->labelWidth(text, metrics, spec);
    m.controlWidth = m.ops->controlWidth(text, metrics, spec);
    m.cell = (spec.cellWidth != 0U)
                 ? static_cast<int>(spec.cellWidth)
                 : (m.labelWidth + m.controlWidth);
    m.indent = inSection ? metrics.sectionIndent : 0;
    m.insetLeft = static_cast<int>(spec.insetLeft);
    m.insetRight = static_cast<int>(spec.insetRight);
    m.section = inSection ? static_cast<std::size_t>(spec.section) : 0U;
    // A port-aligned face may join a row too (ADR-1833): the joined row has
    // no port line of its own and shares that of the row before it.
    m.joined = ((spec.flags & kRowJoined) != 0U) && !shown.empty() &&
               (shown.back().section == m.section);
    shown.push_back(m);
  }
  if (shown.empty()) {
    return out;
  }

  // Phase 2: the lines. A joined row belongs to the line before it.
  std::vector<Line> lines;
  lines.reserve(shown.size());
  for (std::size_t i = 0; i < shown.size(); ++i) {
    if (shown[i].joined) {
      ++lines.back().count;
    } else {
      Line line;
      line.first = i;
      lines.push_back(line);
    }
  }
  for (Line &line : lines) {
    for (std::size_t k = 0; k < line.count; ++k) {
      line.height = std::max(line.height, shown[line.first + k].height);
    }
    // A line stands where its first row's left and its last row's right
    // inset put it (ADR-1850).
    shown[line.first].insetRight = shown[line.first + line.count - 1U].insetRight;
  }
  // A face may state the margin and the gap of the legacy widget's layout
  // (ADR-1806); a line of several rows then uses the gap sideways too.
  const int faceMargin = compact ? -1 : face.bodyMargin();
  const int faceGap = compact ? -1 : face.rowGap();
  const bool framed = faceMargin >= 0;
  const int marginLeft =
      compact ? 0 : (framed ? faceMargin : metrics.marginLeft);
  const int marginTop =
      compact ? 0 : (framed ? faceMargin : metrics.marginTop);
  const int marginRight =
      compact ? 0 : (framed ? faceMargin : metrics.marginRight);
  const int marginBottom =
      compact ? 0 : (framed ? faceMargin : metrics.marginBottom);
  const int spacing =
      compact ? 0 : ((faceGap >= 0) ? faceGap : metrics.rowSpacing);
  const int joinGap = (faceGap >= 0) ? faceGap : metrics.joinSpacing;

  // Phase 3: the width. A section's body is a form of its own (ADR-1306): its
  // labels and its fields line up among themselves, the rows outside a
  // section among theirs. A line of several rows is not a form row.
  struct Columns {
    int label = 0;
    int field = 0;
    int indent = 0;
  };
  std::vector<Columns> columns(1);
  int spanWidth = 0;
  for (const Line &line : lines) {
    Measured &head = shown[line.first];
    if (line.count > 1U) {
      for (std::size_t k = 0; k < line.count; ++k) {
        shown[line.first + k].natural = true;
        shown[line.first + k].twoColumn = false;
      }
      spanWidth = std::max(spanWidth,
                           head.indent + head.insetLeft + head.insetRight +
                               naturalLineWidth(shown, line, joinGap));
      continue;
    }
    const RowSpec &spec = specs[static_cast<std::size_t>(head.index)];
    head.natural = keepsNaturalWidth(spec);
    head.twoColumn = head.ops->twoColumn &&
                     ((spec.flags & kRowWide) == 0U) && !head.natural;
    if (head.twoColumn) {
      if (columns.size() <= head.section) {
        columns.resize(head.section + 1U);
      }
      Columns &col = columns[head.section];
      col.label = std::max(col.label, head.labelWidth);
      col.field = std::max(col.field, head.controlWidth);
      col.indent = std::max(
          col.indent, head.indent + head.insetLeft + head.insetRight);
    } else {
      spanWidth = std::max(spanWidth,
                           head.indent + head.insetLeft + head.insetRight +
                               head.labelWidth + head.controlWidth);
    }
  }

  int innerWidth = spanWidth;
  for (const Columns &col : columns) {
    if ((col.label > 0) || (col.field > 0)) {
      innerWidth = std::max(
          innerWidth,
          col.indent + col.label + metrics.columnSpacing + col.field);
    }
  }
  // The least width of the body is the width of the rows too: the rows of a
  // line share what the legacy widget had beyond its hint (ADR-1806).
  if (face.exactBodyWidth() && (least.width() > 0)) {
    innerWidth = std::max(0, least.width() - marginLeft - marginRight);
  } else {
    innerWidth =
        std::max(innerWidth, least.width() - marginLeft - marginRight);
  }

  // Phase 4: place the lines.
  out.rows.reserve(shown.size());
  int y = marginTop;
  for (const Line &line : lines) {
    const Measured &head = shown[line.first];
    const int x = marginLeft + head.indent + head.insetLeft;
    const int width =
        std::max(0, innerWidth - head.indent - head.insetLeft - head.insetRight);
    // The space the legacy widget left above this line (ADR-1850); a line
    // placed above the flow's bottom does not move the flow back.
    const int flow = y;
    y = std::max(0, y + static_cast<int>(
                           specs[static_cast<std::size_t>(head.index)]
                               .spaceBefore));
    if (line.count > 1U) {
      // What is left over is shared equally by the rows whose width the node
      // did not fix, as a box layout shares it; when every row is fixed the
      // line ends short of the face.
      int flexible = 0;
      for (std::size_t k = 0; k < line.count; ++k) {
        const RowSpec &spec =
            specs[static_cast<std::size_t>(shown[line.first + k].index)];
        flexible +=
            ((spec.fieldWidth == 0U) && (spec.cellWidth == 0U)) ? 1 : 0;
      }
      // A face as wide as its widget (exactBodyWidth) takes the width back from
      // the rows that are not fixed when the line is wider than it, down to a
      // label and a field's least (ADR-1850).
      int extra = width - naturalLineWidth(shown, line, joinGap);
      if (!face.exactBodyWidth()) {
        extra = std::max(0, extra);
      }
      const int sign = (extra < 0) ? -1 : 1;
      const int more = std::abs(extra);
      const int share = (flexible > 0) ? ((more / flexible) * sign) : 0;
      const int remainder = (flexible > 0) ? (more % flexible) : 0;
      constexpr int kLeastControl = 40;
      int at = x;
      int given = 0;
      for (std::size_t k = 0; k < line.count; ++k) {
        const Measured &m = shown[line.first + k];
        const RowSpec &spec = specs[static_cast<std::size_t>(m.index)];
        int cell = m.cell;
        if ((spec.fieldWidth == 0U) && (spec.cellWidth == 0U)) {
          cell += share + ((given < remainder) ? sign : 0);
          cell = std::max(cell, m.labelWidth + kLeastControl);
          ++given;
        }
        const int top = y + ((line.height - m.height) / 2);
        RowGeometry geo;
        geo.index = m.index;
        geo.row = QRect(at, y, cell, line.height);
        if (m.labelWidth > 0) {
          geo.label = QRect(at, top, m.labelWidth, m.height);
        }
        geo.control = QRect(at + m.labelWidth, top,
                            std::max(0, cell - m.labelWidth), m.height);
        m.ops->place(text, metrics, spec, geo);
        out.rows.push_back(geo);
        at += cell + joinGap;
      }
      y = std::max(flow, y + line.height + spacing);
      continue;
    }
    const RowSpec &spec = specs[static_cast<std::size_t>(head.index)];
    RowGeometry geo;
    geo.index = head.index;
    geo.row = QRect(x, y, width, head.height);
    if (head.twoColumn) {
      const int labelColumn = columns[head.section].label;
      // The label column, its text from the left as a form's was (ADR-1851).
      geo.label = QRect(x, y, labelColumn, head.height);
      int fieldX = x + labelColumn + metrics.columnSpacing;
      int fieldWidth = std::max(0, x + width - fieldX);
      // A field of a width the node fixed sits at the right of the line, as
      // the legacy edit did, and does not stretch (ADR-1806).
      if ((spec.fieldWidth != 0U) && (fieldWidth > head.controlWidth)) {
        fieldX += fieldWidth - head.controlWidth;
        fieldWidth = head.controlWidth;
      }
      geo.control = QRect(fieldX, y, fieldWidth, head.height);
    } else if (head.natural) {
      geo.row = QRect(x, y, head.labelWidth + head.controlWidth, head.height);
      geo.control =
          QRect(x + head.labelWidth, y, head.controlWidth, head.height);
    } else {
      geo.control = QRect(x, y, width, head.height);
    }
    head.ops->place(text, metrics, spec, geo);
    out.rows.push_back(geo);
    y = std::max(flow, y + head.height + spacing);
  }
  y -= spacing;
  int bodyHeight = y + marginBottom;
  if (compact) {
    // A port-aligned body is what the legacy widget made of it (see
    // FaceMetrics::portBodyMinHeight); the geometry adds portBodyExtra to the
    // greater of the ports and this body.
    bodyHeight = std::max(bodyHeight, metrics.portBodyMinHeight);
  }
  out.size = QSize(std::max(marginLeft + innerWidth + marginRight,
                            least.width()),
                   std::max(bodyHeight, least.height()));
  return out;
}

const FaceLayoutResult &cachedLayout(const NodeFaceSource &face,
                                     const FaceMetrics &metrics,
                                     const TextMetrics &text) {
  std::unique_ptr<FaceLayoutResult> &slot = face.layoutSlot();
  if (slot && (slot->revision == face.revision()) &&
      (slot->toggles == face.sectionToggles()) &&
      (slot->metrics == &metrics) && (slot->nominal == text.nominal())) {
    return *slot;
  }
  slot = std::make_unique<FaceLayoutResult>(layoutFace(face, metrics, text));
  return *slot;
}

TableGeometry tableGeometry(const NodeFaceSource &face, int row,
                            const QRect &control, const FaceMetrics &metrics) {
  TableGeometry g;
  g.columns = face.tableColumns(row);
  if (g.columns <= 0) {
    g.columns = 0;
    return g;
  }
  g.headerHeight = metrics.labelHeight;
  g.lineHeight = metrics.labelHeight;
  g.visibleLines =
      std::max(0, (control.height() - g.headerHeight) / g.lineHeight);
  std::vector<int> width(static_cast<std::size_t>(g.columns), 0);
  int fixedTotal = 0;
  int shared = 0;
  for (int c = 0; c < g.columns; ++c) {
    const int w = std::max(0, face.tableColumnWidth(row, c));
    width[static_cast<std::size_t>(c)] = w;
    if (w > 0) {
      fixedTotal += w;
    } else {
      ++shared;
    }
  }
  const int share =
      (shared > 0) ? std::max(1, (control.width() - fixedTotal) / shared) : 0;
  g.columnX.assign(static_cast<std::size_t>(g.columns) + 1U, 0);
  int x = 0;
  for (int c = 0; c < g.columns; ++c) {
    g.columnX[static_cast<std::size_t>(c)] = x;
    const int w = width[static_cast<std::size_t>(c)];
    x += (w > 0) ? w : share;
  }
  g.columnX[static_cast<std::size_t>(g.columns)] = x;
  return g;
}

bool tableCellAt(const NodeFaceSource &face, int row, const QRect &control,
                 const QPoint &point, const FaceMetrics &metrics, int *line,
                 int *column) {
  if (!control.contains(point)) {
    return false;
  }
  const TableGeometry g = tableGeometry(face, row, control, metrics);
  if (g.columns == 0) {
    return false;
  }
  const QPoint local = point - control.topLeft();
  int found = -1;
  for (int c = 0; c < g.columns; ++c) {
    if ((local.x() >= g.columnX[static_cast<std::size_t>(c)]) &&
        (local.x() < g.columnX[static_cast<std::size_t>(c) + 1U])) {
      found = c;
      break;
    }
  }
  if (found < 0) {
    return false;
  }
  int at = -1;
  if (local.y() >= g.headerHeight) {
    at = face.tableFirstLine(row) +
         ((local.y() - g.headerHeight) / g.lineHeight);
    if (at >= face.tableLines(row)) {
      return false;
    }
  }
  if (line != nullptr) {
    *line = at;
  }
  if (column != nullptr) {
    *column = found;
  }
  return true;
}

QRect tableCellRect(const TableGeometry &geometry, const QRect &control,
                    int line, int column) {
  if ((column < 0) || (column >= geometry.columns)) {
    return QRect();
  }
  const int left = control.left() +
                   geometry.columnX[static_cast<std::size_t>(column)];
  const int right = control.left() +
                    geometry.columnX[static_cast<std::size_t>(column) + 1U];
  const int top = (line < 0)
                      ? control.top()
                      : (control.top() + geometry.headerHeight +
                         (line * geometry.lineHeight));
  const int height = (line < 0) ? geometry.headerHeight : geometry.lineHeight;
  return QRect(left, top, std::max(0, right - left), height)
      .intersected(control);
}

FaceHit hitTest(const FaceLayoutResult &layout, const QPoint &point) {
  FaceHit hit;
  hit.point = point;
  for (const RowGeometry &geo : layout.rows) {
    if (!geo.row.contains(point)) {
      continue;
    }
    hit.row = geo.index;
    hit.part = FacePart::Body;
    if (geo.stepUp.contains(point)) {
      hit.part = FacePart::StepUp;
    } else if (geo.stepDown.contains(point)) {
      hit.part = FacePart::StepDown;
    } else if (geo.comp[0].contains(point)) {
      hit.part = FacePart::CompX;
    } else if (geo.comp[1].contains(point)) {
      hit.part = FacePart::CompY;
    } else if (geo.comp[2].contains(point)) {
      hit.part = FacePart::CompZ;
    } else if (geo.disclosure.contains(point)) {
      hit.part = FacePart::Disclosure;
    } else if (geo.control.contains(point)) {
      hit.part = FacePart::Control;
    } else if (geo.label.contains(point)) {
      hit.part = FacePart::Label;
    } else {
      hit.part = FacePart::Body;
    }
    return hit;
  }
  return hit;
}

} // namespace QtNodes
