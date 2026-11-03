// SectionRowPainter.cpp - the Section row: a collapsible header (CICADA,
// ADR-1306, ADR-1631). Text: arrow, group name and its row count.
#include "RowOps.hpp"

#include <QtGui/QFont>

namespace QtNodes {

namespace {

constexpr int kDisclosureWidth = 14;

int height(const FaceMetrics &metrics, const RowSpec &spec) {
  static_cast<void>(spec);
  return metrics.sectionHeight;
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
  return kDisclosureWidth + text.advance(spec.label, true) + metrics.cellPad;
}

void place(const TextMetrics &text, const FaceMetrics &metrics,
           const RowSpec &spec, RowGeometry &geometry) {
  static_cast<void>(text);
  static_cast<void>(metrics);
  static_cast<void>(spec);
  geometry.disclosure = QRect(geometry.control.left(), geometry.control.top(),
                              kDisclosureWidth, geometry.control.height());
}

void paint(const RowPaintArgs &args) {
  constexpr int kHoverAlpha = 40;
  constexpr int kHoverGrey = 128;
  if (args.hovered) {
    args.painter.fillRect(args.geometry.control,
                          QColor(kHoverGrey, kHoverGrey, kHoverGrey,
                                 kHoverAlpha));
  }
  const bool open = args.face.sectionExpanded(args.spec.section);
  QFont font = args.painter.font();
  font.setBold(true);
  args.painter.setFont(font);
  const QString glyph =
      QString(QChar(open ? 0x25BE : 0x25B8)) + QLatin1Char(' ');
  paintCellText(args.painter, args.geometry.control.adjusted(4, 0, 0, 0),
                glyph + args.spec.label, Qt::AlignVCenter | Qt::AlignLeft,
                faceTextColor(true));
  font.setBold(false);
  args.painter.setFont(font);
}

} // namespace

const RowOps kSectionRowOps = {height, labelWidth, controlWidth, false, place,
                               paint};

} // namespace QtNodes
