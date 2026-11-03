// RowOps.hpp - the per-kind operations of a face row (CICADA, ADR-1631).
//
// Every RowKind has one translation unit, src/rows/<Kind>RowPainter.cpp, that
// defines its RowOps. RowOpsTable.cpp maps a kind to them. A converted node
// never edits this file or the enum: S4 adds or changes a row painter's .cpp.
#pragma once

#include "FaceLayout.hpp"
#include "NodeFace.hpp"
#include "NodeFacePainter.hpp"

#include <QtCore/QRect>
#include <QtGui/QColor>
#include <QtGui/QPainter>

namespace QtNodes {

/// What a row painter is given.
struct RowPaintArgs {
  QPainter &painter;
  const RowSpec &spec;
  const RowGeometry &geometry;
  const NodeFaceSource &face;
  const FacePaintState &state;
  /// Whether the row may be edited now.
  bool enabled;
  /// Whether the pointer is over the row.
  bool hovered;
};

/// What a RowKind decides: its size, its parts and its picture.
struct RowOps {
  /// The row's height.
  int (*height)(const FaceMetrics &metrics, const RowSpec &spec);
  /// The width of the label cell, padding included; 0 for none.
  int (*labelWidth)(const TextMetrics &text, const FaceMetrics &metrics,
                    const RowSpec &spec);
  /// The least width of the control; 0 for a row that has none.
  int (*controlWidth)(const TextMetrics &text, const FaceMetrics &metrics,
                      const RowSpec &spec);
  /// Whether the label and the control share a column pair with other rows
  /// (a form row) and not a line of their own.
  bool twoColumn;
  /// Fills the parts (arrows, components, triangle) once `label` and
  /// `control` are set.
  void (*place)(const TextMetrics &text, const FaceMetrics &metrics,
                const RowSpec &spec, RowGeometry &geometry);
  /// Paints the row.
  void (*paint)(const RowPaintArgs &args);
};

/// \brief The operations of a kind.
/// \param kind A row kind.
/// \return Its operations; the Label's for a value that is not a kind.
const RowOps &rowOps(RowKind kind);

/// The painters' operations, one per translation unit.
extern const RowOps kLabelRowOps;
extern const RowOps kBoolRowOps;
extern const RowOps kIntRowOps;
extern const RowOps kDoubleRowOps;
extern const RowOps kEnumRowOps;
extern const RowOps kStringRowOps;
extern const RowOps kVec3RowOps;
extern const RowOps kButtonRowOps;
extern const RowOps kSectionRowOps;
extern const RowOps kSliderRowOps;
extern const RowOps kSwatchRowOps;
extern const RowOps kTableRowOps;
extern const RowOps kPortAlignRowOps;

/// The text colour of the legacy node labels.
inline QColor faceTextColor(bool enabled) {
  return enabled ? QColor(220, 220, 220) : QColor(140, 140, 140);
}

/// The fill of an inline text field (CircleInlineEditors.h editStyle).
inline QColor faceEditFill(bool enabled) {
  return enabled ? QColor(245, 245, 245, 180) : QColor(210, 210, 210, 140);
}

/// The text colour of an inline text field.
inline QColor faceEditText(bool enabled) {
  return enabled ? QColor(40, 40, 40) : QColor(100, 100, 100);
}

/// The text of a label cell.
void paintCellText(QPainter &painter, const QRect &cell, const QString &text,
                   int flags, const QColor &color);

/// A flat cell in the node's state colour, as the style sheet drew one.
void paintCell(QPainter &painter, const QRect &cell, const QColor &fill);

/// The two stacked step arrows (or the combo arrow) inside a field.
void paintStepArrows(QPainter &painter, const QRect &up, const QRect &down,
                     const QColor &edge, bool enabled);

/// The width of a label cell: the label, its colon and the padding.
int labelCellWidth(const TextMetrics &text, const FaceMetrics &metrics,
                   const RowSpec &spec);

/// The width of a number field as a spin box sizes itself: the widest of the
/// texts of its two range ends, its units, the arrows and the padding.
int numberFieldWidth(const TextMetrics &text, const FaceMetrics &metrics,
                     const RowSpec &spec, bool integer);

/// The label cell of a two-column row.
void paintRowLabel(const RowPaintArgs &args);

/// The field cell of a two-column row, with its text.
void paintFieldCell(const RowPaintArgs &args, const QString &text, int align);

/// Whether a row is an inline row (kRowInline): a port-inline node's editor.
inline bool isInlineRow(const RowSpec &spec) {
  return (spec.flags & kRowInline) != 0U;
}

/// The width of an inline row's field: the row's own, else \p fallback.
inline int inlineFieldWidth(const RowSpec &spec, int fallback) {
  return (spec.fieldWidth != 0U) ? static_cast<int>(spec.fieldWidth) : fallback;
}

/// The field of an inline row inside the row's control cell: as high as the
/// legacy edit (the port line less 8 px), centred in the line.
QRect inlineFieldRect(const FaceMetrics &metrics, const QRect &cell);

/// An inline field as CircleInlineEditors.h editStyle drew it: a light fill, a
/// 1 px dark border, the text in dark.
void paintInlineField(const RowPaintArgs &args, const QString &text, int align);

} // namespace QtNodes
