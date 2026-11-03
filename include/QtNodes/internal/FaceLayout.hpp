// FaceLayout.hpp - where each row of a node's face goes (CICADA, ADR-1631).
//
// A pure function of (rows, visibility, open sections, width, metrics): no
// widget, no scene, the same result with the editor hidden or shown.
#pragma once

#include "Export.hpp"
#include "NodeFace.hpp"

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>

#include <cstdint>
#include <vector>

namespace QtNodes {

/// \brief What the layout asks of the font.
///
/// A QFontMetrics when a GUI application exists; a fixed nominal advance when
/// there is none, so that a size computed headless is nominal.
class NODE_EDITOR_PUBLIC TextMetrics {
public:
  virtual ~TextMetrics();
  /// \brief The width of \p text.
  /// \param text The text.
  /// \param bold Whether it is set in the bold face.
  /// \return Pixels.
  virtual int advance(const QString &text, bool bold = false) const = 0;
  /// \brief The height of one line.
  /// \return Pixels.
  virtual int lineHeight() const = 0;
  /// \brief Whether the numbers are nominal (no real font behind them).
  /// \return true for a headless measure.
  virtual bool nominal() const { return false; }

  /// \brief The metrics of the application font, or the nominal ones when
  ///        there is no GUI application.
  /// \return A reference to a process-wide instance.
  static const TextMetrics &application();
};

/// \brief Every constant of the layout, in scene units. Calibrated against
///        the legacy nodes (tests/data/canvas/legacy_reference_sizes.json).
struct NODE_EDITOR_PUBLIC FaceMetrics {
  /// Margins of the body (left, top, right, bottom).
  int marginLeft = 4;
  int marginTop = 2;
  int marginRight = 4;
  int marginBottom = 2;
  /// Space between rows.
  int rowSpacing = 3;
  /// Space between the label cell and the field cell.
  int columnSpacing = 3;
  /// Space between the rows of one line (kRowJoined) of a face that does not
  /// state its own (NodeFaceSource::rowGap; ADR-1806).
  int joinSpacing = 4;
  /// Indent of the rows of a section.
  int sectionIndent = 12;
  /// Horizontal padding inside a label cell and a field cell.
  int cellPad = 8;
  /// Heights of the kinds.
  int textEditHeight = 22;
  int spinHeight = 25;
  int comboHeight = 22;
  int checkHeight = 21;
  int buttonHeight = 24;
  int sectionHeight = 22;
  /// The line of a Label row: one text line and its padding.
  int labelHeight = 20;
  /// The height of a row that sits on a port line (`PortAlign`, `Vec3`): one
  /// port step, line height + 10, as the legacy editors' `rowHeight()`.
  int portRowHeight = 26;
  /// Width of the step arrows of a number field and of a combo's arrow.
  int arrowWidth = 17;
  /// Width and height of the check box square.
  int checkBox = 14;
  /// Least width of a field cell.
  int minFieldWidth = 70;
  /// Width of one X, Y or Z text edit of a Vec3 row.
  int vecEditWidth = 55;
  /// Space between the parts of a Vec3 row.
  int vecSpacing = 1;
  /// Padding to the right of an axis letter: the legacy label cell of the
  /// real application is the letter and 12 px (S0 sizes: a Vec3 row is
  /// 231 px, 3 x (letter + 12 + 55) + 5).
  int vecLabelPad = 12;
  /// The least height of a port-aligned body, before `portBodyExtra`: what
  /// Qt's adjustSize() gives a top-level widget whose layout has a stretch.
  int portBodyMinHeight = 100;
  /// What the legacy proxy added to that body: the node's own size less its
  /// caption, re-applied as the widget's least height (NodeGraphicsObject::
  /// embedQWidget), so a body is that much taller than its rows.
  int portBodyExtra = 20;
  /// Default widths of the fields of an inline row, by kind.
  int inlineDoubleWidth = 90;
  int inlineIntWidth = 70;
  int inlineEnumWidth = 120;
  /// Width the text of a Label row may take before it is cut.
  int maxLabelWidth = 520;

  /// \brief The constants that follow the font (heights built on it).
  /// \param text The font.
  /// \return Metrics with the heights of \p text's line.
  static FaceMetrics forFont(const TextMetrics &text);
};

/// \brief The parts of a row a pointer can land on.
enum class FacePart : std::uint8_t {
  /// Outside every row.
  None,
  /// A row, between its parts: drags the node.
  Body,
  /// The label.
  Label,
  /// The field, the check box, the button, the swatch or the table.
  Control,
  /// The arrow that raises a number.
  StepUp,
  /// The arrow that lowers a number.
  StepDown,
  /// The X field of a Vec3 row.
  CompX,
  /// The Y field of a Vec3 row.
  CompY,
  /// The Z field of a Vec3 row.
  CompZ,
  /// The disclosure triangle of a Section row.
  Disclosure,
};

/// \brief A pointer position over a face.
struct FaceHit {
  /// The row's index in rows(); -1 for none.
  int row = -1;
  /// The part of it.
  FacePart part = FacePart::None;
  /// The point hit, in body coordinates (a Table row finds its cell by it).
  QPoint point;
};

/// \brief Where the parts of a Table row's table are (ADR-1804).
struct TableGeometry {
  int columns = 0;
  int headerHeight = 0;
  int lineHeight = 0;
  /// How many lines fit below the header.
  int visibleLines = 0;
  /// columns + 1 entries: the left edge of each column, then the right edge
  /// of the last, relative to the control's left.
  std::vector<int> columnX;
};

/// \brief The geometry of the table of a Table row.
/// \param face The face.
/// \param row Index into the face's rows.
/// \param control The row's control rectangle (body coordinates).
/// \param metrics The face metrics.
/// \return The geometry; columns 0 when the row has no table of its own.
NODE_EDITOR_PUBLIC TableGeometry tableGeometry(const NodeFaceSource &face,
                                               int row, const QRect &control,
                                               const FaceMetrics &metrics);

/// \brief The cell at a point of a Table row.
/// \param point In body coordinates.
/// \param line Set to the line (0 up below the header), -1 for the header.
/// \param column Set to the column.
/// \return true when the point is on a cell of the table.
NODE_EDITOR_PUBLIC bool tableCellAt(const NodeFaceSource &face, int row,
                                    const QRect &control, const QPoint &point,
                                    const FaceMetrics &metrics, int *line,
                                    int *column);

/// \brief The rectangle of a cell (body coordinates).
/// \param line The line shown, 0 up below the header (the first line shown),
///        -1 for the header.
/// \param column The column.
NODE_EDITOR_PUBLIC QRect tableCellRect(const TableGeometry &geometry,
                                       const QRect &control, int line,
                                       int column);

/// \brief Where one row is, in body coordinates (the body's own origin).
struct RowGeometry {
  /// The row's index in rows().
  int index = -1;
  /// The whole row.
  QRect row;
  /// The label cell; empty when the row has none.
  QRect label;
  /// The control: the field, the box, the button.
  QRect control;
  /// The arrows of a number field; empty for the other kinds.
  QRect stepUp;
  QRect stepDown;
  /// The three fields of a Vec3 row, and the axis letters before them.
  QRect comp[3];
  QRect axis[3];
  /// The triangle of a Section row.
  QRect disclosure;
};

/// \brief A laid-out face.
struct NODE_EDITOR_PUBLIC FaceLayoutResult {
  /// The rows shown, top to bottom.
  std::vector<RowGeometry> rows;
  /// The size of the body.
  QSize size;
  /// The revision of the face this was built for.
  std::uint64_t revision = 0;
  /// The sections toggled when it was built.
  std::uint64_t toggles = 0;
  /// The metrics it was built with, by identity.
  const FaceMetrics *metrics = nullptr;
  /// Whether the text metrics were nominal.
  bool nominal = false;
};

/// \brief Lay a face out.
/// \param face The rows.
/// \param metrics The constants.
/// \param text The font.
/// \return The geometry; the size of an empty face is 0 x 0.
NODE_EDITOR_PUBLIC FaceLayoutResult layoutFace(const NodeFaceSource &face,
                                               const FaceMetrics &metrics,
                                               const TextMetrics &text);

/// \brief The layout of a face, rebuilt only when its revision, its open
///        sections or the metrics changed.
/// \param face The rows; the layout is cached in it.
/// \param metrics The constants; must outlive the cache (use metrics()).
/// \param text The font.
/// \return A reference valid until the next call for the same face.
NODE_EDITOR_PUBLIC const FaceLayoutResult &
cachedLayout(const NodeFaceSource &face, const FaceMetrics &metrics,
             const TextMetrics &text);

/// \brief The constants of the application font, built once.
/// \return A process-wide instance.
NODE_EDITOR_PUBLIC const FaceMetrics &faceMetrics();

/// \brief What is under a point.
/// \param layout The laid-out face.
/// \param point In body coordinates.
/// \return The row and the part; row -1 and None outside every row.
NODE_EDITOR_PUBLIC FaceHit hitTest(const FaceLayoutResult &layout,
                                   const QPoint &point);

} // namespace QtNodes
