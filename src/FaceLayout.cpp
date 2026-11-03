// FaceLayout.cpp - the layout of a node's face (CICADA, ADR-1631).
#include "FaceLayout.hpp"

#include "rows/RowOps.hpp"

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>

#include <algorithm>
#include <cstddef>

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
  int indent = 0;
  bool twoColumn = false;
  bool natural = false;
  std::size_t section = 0;
};

/// A row that keeps its own width and is not stretched to the body's.
bool keepsNaturalWidth(const RowSpec &spec) {
  return (spec.kind == RowKind::Vec3) || (spec.kind == RowKind::PortAlign) ||
         ((spec.flags & kRowInline) != 0U);
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

  const std::shared_ptr<const std::vector<RowSpec>> rowsPtr = face.rows();
  if (!rowsPtr) {
    return out;
  }
  const std::vector<RowSpec> &specs = *rowsPtr;
  // The rows of a port-aligned face (the Point, the Circle family) fill the
  // body with no margin and no gap: each sits on a port line.
  const bool compact = face.alignsToPorts();
  const int marginLeft = compact ? 0 : metrics.marginLeft;
  const int marginTop = compact ? 0 : metrics.marginTop;
  const int marginRight = compact ? 0 : metrics.marginRight;
  const int marginBottom = compact ? 0 : metrics.marginBottom;
  const int spacing = compact ? 0 : metrics.rowSpacing;

  std::vector<Measured> shown;
  shown.reserve(specs.size());
  // A section's body is a form of its own (ADR-1306): its labels and its
  // fields line up among themselves, the rows outside a section among theirs.
  struct Columns {
    int label = 0;
    int field = 0;
    int indent = 0;
  };
  std::vector<Columns> columns(1);
  int spanWidth = 0;
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
    // A port-aligned face keeps each row on its port line: a port step high.
    m.height = compact ? metrics.portRowHeight : m.ops->height(metrics, spec);
    m.labelWidth = m.ops->labelWidth(text, metrics, spec);
    m.controlWidth = m.ops->controlWidth(text, metrics, spec);
    m.indent = inSection ? metrics.sectionIndent : 0;
    m.natural = keepsNaturalWidth(spec);
    m.twoColumn =
        m.ops->twoColumn && ((spec.flags & kRowWide) == 0U) && !m.natural;
    m.section = inSection ? static_cast<std::size_t>(spec.section) : 0U;
    if (m.twoColumn) {
      if (columns.size() <= m.section) {
        columns.resize(m.section + 1U);
      }
      Columns &col = columns[m.section];
      col.label = std::max(col.label, m.labelWidth);
      col.field = std::max(col.field, m.controlWidth);
      col.indent = std::max(col.indent, m.indent);
    } else {
      spanWidth = std::max(spanWidth, m.indent + m.labelWidth + m.controlWidth);
    }
    shown.push_back(m);
  }
  if (shown.empty()) {
    return out;
  }

  int innerWidth = spanWidth;
  for (const Columns &col : columns) {
    if ((col.label > 0) || (col.field > 0)) {
      innerWidth = std::max(
          innerWidth,
          col.indent + col.label + metrics.columnSpacing + col.field);
    }
  }

  out.rows.reserve(shown.size());
  int y = marginTop;
  for (const Measured &m : shown) {
    const RowSpec &spec = specs[static_cast<std::size_t>(m.index)];
    RowGeometry geo;
    geo.index = m.index;
    const int x = marginLeft + m.indent;
    const int width = innerWidth - m.indent;
    geo.row = QRect(x, y, width, m.height);
    if (m.twoColumn) {
      const int labelColumn = columns[m.section].label;
      geo.label = QRect(x + labelColumn - m.labelWidth, y, m.labelWidth,
                        m.height);
      const int fieldX = x + labelColumn + metrics.columnSpacing;
      geo.control = QRect(fieldX, y, std::max(0, x + width - fieldX),
                          m.height);
    } else if (m.natural) {
      geo.row = QRect(x, y, m.labelWidth + m.controlWidth, m.height);
      geo.control = QRect(x + m.labelWidth, y, m.controlWidth, m.height);
    } else {
      geo.control = QRect(x, y, width, m.height);
    }
    m.ops->place(text, metrics, spec, geo);
    out.rows.push_back(geo);
    y += m.height + spacing;
  }
  y -= spacing;
  int bodyHeight = y + marginBottom;
  if (compact) {
    // A port-aligned body is what the legacy widget made of it (see
    // FaceMetrics::portBodyMinHeight); the geometry adds portBodyExtra to the
    // greater of the ports and this body.
    bodyHeight = std::max(bodyHeight, metrics.portBodyMinHeight);
  }
  const QSize least = face.minBodySize();
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

FaceHit hitTest(const FaceLayoutResult &layout, const QPoint &point) {
  FaceHit hit;
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
