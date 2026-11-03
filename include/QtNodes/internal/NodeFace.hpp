// NodeFace.hpp - the rows a node paints in its body (CICADA, ADR-1631).
//
// A node has no QWidget. Its body is a list of rows, described by a
// NodeFaceSource, laid out by FaceLayout and painted by NodeFacePainter, one
// painter per RowKind. Qt Core and Gui types only: the application's
// parameter system (cicada::param) adapts to it, the fork knows nothing of it.
#pragma once

#include "Definitions.hpp"
#include "Export.hpp"

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace QtNodes {

class AbstractGraphModel;
struct FaceLayoutResult;

/// \brief The kinds of row a face has. The set is closed and every kind has a
///        painter file of its own (src/rows/<Kind>RowPainter.cpp), so a
///        converted node adds a node edit, never an edit to this enum.
enum class RowKind : std::uint8_t {
  /// A line of text (status, a note).
  Label,
  /// A check box with its label.
  Bool,
  /// A whole number: label, text field and step arrows.
  Int,
  /// A real number: label, text field, step arrows, units.
  Double,
  /// One of a list: label and a field that opens a pop-up list.
  Enum,
  /// Text: label and a text field.
  String,
  /// Three numbers X, Y and Z in one line.
  Vec3,
  /// A push button.
  Button,
  /// A collapsible section header; the rows of the section follow it.
  Section,
  /// A real number on a slider.
  Slider,
  /// A colour swatch.
  Swatch,
  /// A read-only preview of a table; its editor is the overlay.
  Table,
  /// An empty row that keeps the rows below it on their port lines.
  PortAlign,
};

/// \brief How many RowKind values exist.
inline constexpr std::size_t kRowKindCount = 13U;

/// \brief Bits of RowSpec::flags.
enum RowFlag : std::uint8_t {
  /// The row is shown and never edited (a wired input, a status).
  kRowDisabled = 1U,
  /// The row spans the label and field columns.
  kRowWide = 2U,
  /// The label is rich text (links); the row paints a cached document.
  kRowRich = 4U,
  /// The number has no declared range: its text is the short `%g` form.
  kRowUnbounded = 8U,
  /// The row is one of a port-inline node (ADR-1754): it looks and measures as
  /// the legacy inline editors did (a light field of a fixed width, no label
  /// cell, no arrows on a real number), and keeps its own width.
  kRowInline = 16U,
  /// A Table row of the face's own size (the Panel, ADR-1804): the control is
  /// RowSpec::min by RowSpec::max pixels, and the table in it is the face's
  /// (tableColumns() and the functions after it).
  kRowSized = 32U,
  /// The row sits on the line of the row before it, to its right (ADR-1806):
  /// the legacy action widgets put a button, a status and a field on one
  /// line. The line is as high as its highest row and as wide as the face;
  /// the rows share the width left over equally, as a box layout did.
  kRowJoined = 64U,
};

/// \brief What one row is. Shared per node type, never per node.
struct RowSpec {
  /// How the row looks and behaves.
  RowKind kind = RowKind::Label;
  /// Stable id (the parameter id for a parameter row).
  QString id;
  /// The text beside the control.
  QString label;
  /// The tooltip.
  QString tooltip;
  /// Units shown after a number.
  QString units;
  /// Least value; lowest() when the row has no lower bound.
  double min = std::numeric_limits<double>::lowest();
  /// Greatest value; max() when the row has no upper bound.
  double max = std::numeric_limits<double>::max();
  /// One step of the arrows and the wheel; 0 means the kind's default.
  double step = 0.0;
  /// Enum: the stored ids.
  QStringList choices;
  /// Enum: the words shown, parallel to \c choices.
  QStringList choiceLabels;
  /// The input port whose line the row sits on; -1 for none.
  int port = -1;
  /// A Section row: its own number (1 and up). Any other row: the number of
  /// the section it belongs to, 0 when it is outside every section.
  std::uint16_t section = 0;
  /// A combination of RowFlag bits.
  std::uint8_t flags = 0;
  /// An inline row: the width of the field in pixels; 0 for the kind's own.
  /// A Button, a Bool, a String and a Slider row also take it (ADR-1806),
  /// as the width of the control a legacy widget fixed.
  std::uint16_t fieldWidth = 0;
  /// The height of the row in pixels; 0 for the kind's own (ADR-1806: a
  /// legacy button in a stack was 20 px high, the slider 19).
  std::uint8_t rowHeight = 0;
};

/// \brief Who is editing, so that a form on the same value skips its own
///        echo (ADR-0682). Compared by address only, never dereferenced.
struct EditSource {
  /// The editing control; null for none.
  const void *origin = nullptr;
};

/// \brief How a number row shows its value: shown = stored * scale.
struct RowDisplay {
  /// Display units per stored unit; greater than 0.
  double scale = 1.0;
  /// The unit name after the number; empty for none.
  QString suffix;
};

/// \brief The state and the behaviour of one node's rows.
///
/// One per node. Its rows() may be shared by every node of a type. Everything
/// runs on the GUI thread. A face holds the node's state of the sections
/// (open or shut), which is display state: never saved, never undone.
class NODE_EDITOR_PUBLIC NodeFaceSource {
public:
  NodeFaceSource();
  virtual ~NodeFaceSource();
  NodeFaceSource(const NodeFaceSource &) = delete;
  NodeFaceSource &operator=(const NodeFaceSource &) = delete;
  NodeFaceSource(NodeFaceSource &&) = delete;
  NodeFaceSource &operator=(NodeFaceSource &&) = delete;

  /// \brief The rows, in order.
  /// \return The row list; never null.
  virtual std::shared_ptr<const std::vector<RowSpec>> rows() const = 0;

  /// \brief Whether a row is shown now (a `visibleWhen` rule).
  /// \param row Index into rows().
  /// \return true unless the row is hidden; the base shows every row.
  virtual bool rowVisible(int row) const;

  /// \brief Whether a row may be edited now.
  /// \param row Index into rows().
  /// \return false for a row a wire drives; the base answers by RowFlag.
  virtual bool rowEnabled(int row) const;

  /// \brief The value of a row.
  /// \param row Index into rows().
  /// \return Bool: bool. Int: int. Double: double, in stored units. Enum: the
  ///         stored id. String: text. Vec3: a list of three doubles.
  virtual QVariant value(int row) const = 0;

  /// \brief Change a value, the way a person's edit does.
  /// \param row Index into rows().
  /// \param value The new value, typed as value() returns it.
  /// \param source The editor, for echo suppression.
  /// \return false when the value was refused (nothing changed; the row
  ///         paints the stored value).
  virtual bool setValue(int row, const QVariant &value, EditSource source) = 0;

  /// \brief Press a Button row.
  /// \param row Index into rows().
  /// \return true when something was invoked.
  virtual bool invoke(int row);

  /// \brief A number that changes when the rows, their visibility or their
  ///        sizes change; a layout is rebuilt when it does.
  /// \return The revision.
  virtual std::uint64_t revision() const = 0;

  /// \brief The ids an Enum row offers now. A list that depends on the node's
  ///        state (a CASTOR face, a thread size) is the face's to give, when
  ///        the pop-up opens, not at paint.
  /// \param row Index into rows().
  /// \return The stored ids; the base answers the row's own `choices`.
  virtual QStringList choicesOf(int row) const;

  /// \brief The words of choicesOf(), parallel to it.
  /// \param row Index into rows().
  /// \return The words shown; the base answers the row's own `choiceLabels`.
  virtual QStringList choiceLabelsOf(int row) const;

  /// \brief The least size of the body: a legacy widget never shrank below
  ///        what Qt's adjustSize() gave a top-level widget with a stretch.
  /// \return 0 x 0 for none (the default).
  virtual QSize minBodySize() const;

  /// \brief The margin the legacy widget's layout set on its four sides
  ///        (ADR-1806).
  /// \return Pixels; -1 for the metrics' own margins (the default).
  virtual int bodyMargin() const;

  /// \brief The space the legacy widget's layout left between its rows, and
  ///        between the rows of one line (ADR-1806).
  /// \return Pixels; -1 for the metrics' own spacing (the default).
  virtual int rowGap() const;

  /// \brief The scale and suffix a number row shows.
  /// \param row Index into rows().
  /// \return The display unit; the base shows stored values with the row's
  ///         own units.
  virtual RowDisplay display(int row) const;

  /// \brief The text a row paints.
  /// \param row Index into rows().
  /// \param component 0 for the row, 0 to 2 for a Vec3 component.
  /// \return The text, formatted as the legacy editor showed it.
  virtual QString displayText(int row, int component = 0) const;

  /// \brief Why the last value offered to a row was refused.
  /// \param row Index into rows().
  /// \return The reason, worded for the person ("Radius = 0: must lie
  ///         between ..."); empty when the row's last write was accepted.
  ///         Shown as a tooltip at the row (ADR-1253).
  virtual QString refusalOf(int row) const;

  /// \name The content of a Table row (the Panel, ADR-1804).
  /// @{
  /// \brief How many columns the table of a Table row has.
  /// \param row Index into rows().
  /// \return 0 for a row with no table of its own: it paints displayText().
  virtual int tableColumns(int row) const;
  /// \brief How many lines the table has below its header.
  /// \param row Index into rows().
  virtual int tableLines(int row) const;
  /// \brief The first line shown (the table is scrolled by display state).
  /// \param row Index into rows().
  virtual int tableFirstLine(int row) const;
  /// \brief The header text of a column.
  virtual QString tableHeader(int row, int column) const;
  /// \brief The text of a cell.
  /// \param line 0 up, below the header.
  virtual QString tableCell(int row, int line, int column) const;
  /// \brief The width of a column in pixels; 0 shares what is left.
  virtual int tableColumnWidth(int row, int column) const;
  /// \brief Whether a cell may be edited now.
  virtual bool tableCellEditable(int row, int line, int column) const;
  /// \brief Write a cell, the way a person's edit does.
  /// \return false when the text was refused (nothing changed).
  virtual bool setTableCell(int row, int line, int column, const QString &text,
                            EditSource source);
  /// \brief Scroll the table by \p lines (negative up).
  /// \return true when the first line moved.
  virtual bool scrollTable(int row, int lines);
  /// @}

  /// \brief The person dragged the node's resize handle: a node whose body
  ///        has a size of its own takes it.
  /// \param size The body size asked for, margins included.
  /// \param final true at the release of the drag.
  /// \return true when the face took the size (it then changes its rows).
  virtual bool resizeBody(const QSize &size, bool final);

  /// \brief Whether the body sits against the first port line instead of
  ///        being centred between the caption and the bottom. True for the
  ///        port-aligned rows of the Point and the Circle family.
  /// \return false by default.
  virtual bool alignsToPorts() const;

  /// \brief Whether a section is open.
  /// \param section A Section row's number (RowSpec::section); 0 is always
  ///        open.
  /// \return true when its rows are shown. The first section starts open and
  ///         the rest shut (ADR-1306).
  bool sectionExpanded(int section) const;

  /// \brief Open or shut a section. Display state: nothing is recomputed.
  /// \param section The section's number.
  void toggleSection(int section);

  /// \brief What toggleSection() has done, for the layout cache.
  /// \return A mask of the sections that differ from their starting state.
  std::uint64_t sectionToggles() const { return sectionToggles_; }

  /// \brief The layout last built for this face (FaceLayout.cpp owns it).
  /// \return A reference to the cache slot.
  std::unique_ptr<FaceLayoutResult> &layoutSlot() const;

  /// \brief The face of a node with no face of its own: one Label row,
  ///        "(not converted)".
  /// \return The shared placeholder.
  static NodeFaceSource &placeholder();

private:
  std::uint64_t sectionToggles_ = 0;
  mutable std::unique_ptr<FaceLayoutResult> layout_;
};

/// \brief The face a node paints.
/// \param model The graph model.
/// \param node The node.
/// \return The delegate's face, or the placeholder when it has none; the
///         placeholder when \p model is not a DataFlowGraphModel.
NODE_EDITOR_PUBLIC NodeFaceSource &faceOf(AbstractGraphModel &model,
                                          NodeId node);

} // namespace QtNodes
