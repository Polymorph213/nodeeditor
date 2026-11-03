// TableFace.hpp - a Table row that is a spreadsheet, not a preview (CICADA,
// ADR-1872).
//
// A face source that also derives from TableFaceSource gives its Table row the
// look and the gestures of the Panel's old widget: a tinted table, a header
// with a sort mark, styled cells (a branch line, a selected cell, an invalid
// cell), a nickname strip, a caption, a toolbar, a footer, and the calls a
// press, a key or a drag make. The row paints and hit-tests the same table
// whether or not the source derives from this; the painter finds it with
// tableFaceOf(). Header-only: nothing here owns a widget.
#pragma once

#include "FaceLayout.hpp"
#include "NodeFace.hpp"

#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>

#include <algorithm>
#include <cstdint>

class QMenu;

namespace QtNodes {

/// \brief The part of a spreadsheet a tooltip is asked about.
enum class TablePart : std::uint8_t { Cell, Header, Toolbar, Footer };

/// \brief What the whole table paints with.
struct TableLook
{
    QColor background;
    QColor header;
    QColor headerText;
    QColor grid;
    QColor text;
    QColor dimmed;
    QColor selection{0x4A, 0x6A, 0x8A};
    QColor selectionText{0xFF, 0xFF, 0xFF};
    QColor error{0xE0, 0x50, 0x50};
    /// The table's font; the application's when it is not set.
    QFont font;
    bool hasFont = false;
};

/// \brief What one cell paints with.
struct TableCellLook
{
    /// Invalid: the table's background.
    QColor background;
    /// Invalid: the table's text colour.
    QColor foreground;
    Qt::Alignment alignment = Qt::AlignLeft;
    bool bold = false;
    bool monospace = false;
    bool selected = false;
    /// The cell the keyboard acts on.
    bool current = false;
    /// An invalid hand-built cell: outlined.
    bool error = false;
    /// The line is a branch header: its first cell spans the table.
    bool branch = false;
    /// A branch line's dimmed text after its path.
    QString branchRest;
};

/// \brief The look and the gestures of a spreadsheet Table row. Every call
///        runs on the GUI thread.
class TableFaceSource
{
public:
    TableFaceSource() = default;
    virtual ~TableFaceSource() = default;
    TableFaceSource(const TableFaceSource &) = delete;
    TableFaceSource &operator=(const TableFaceSource &) = delete;
    TableFaceSource(TableFaceSource &&) = delete;
    TableFaceSource &operator=(TableFaceSource &&) = delete;

    /// \name Look
    /// \{
    virtual TableLook tableLook(int row) const = 0;
    virtual TableCellLook tableCellLook(int row, int line, int column) const = 0;
    /// A strip above the table; empty for none.
    virtual QString tableNickname(int row) const;
    /// A strip under the nickname; empty for none.
    virtual QString tableCaption(int row) const;
    /// Whether the + and x buttons show above the table.
    virtual bool tableToolbar(int row) const;
    /// The footer's left text, and its right text with the colour of its
    /// worst count (invalid: dimmed).
    virtual QString tableFooterLeft(int row) const;
    virtual QString tableFooterRight(int row) const;
    virtual QColor tableFooterColour(int row) const;
    /// What shows when the canvas is zoomed far out.
    virtual QString tableSummary(int row) const;
    /// The sorted column (-1 for none) and its order (1, -1, 0).
    virtual int tableSortColumn(int row) const;
    virtual int tableSortOrder(int row) const;
    /// \}

    /// \name Gestures
    /// \{
    /// A press (or a drag step with \p drag) on a cell: select it, \p modifiers
    /// extend (Shift) or add (Ctrl).
    virtual void tableSelect(
        int row, int line, int column, Qt::KeyboardModifiers modifiers, bool drag);
    /// A click on a header cell.
    virtual void tableHeaderClicked(int row, int column);
    /// A header's edge dragged: the column is \p width wide; \p final at the
    /// release.
    virtual void tableResizeColumn(int row, int column, int width, bool final);
    /// A click on the toolbar: button 0 (+) fills \p menu, button 1 (x) deletes
    /// the selected rows or columns. Returns true when \p menu was filled.
    virtual bool tableToolbarClick(int row, int button, QMenu &menu);
    /// The selected cells as the text a copy puts on the clipboard.
    virtual QString tableSelectionText(int row) const;
    /// The cell the keyboard acts on, as a line shown and a column.
    virtual bool tableCurrent(int row, int *line, int *column) const;
    /// Move the current cell; \p extend selects the range.
    virtual bool tableMoveCurrent(int row, int lines, int columns, bool extend);
    /// Enter committed a cell: go down a row, adding one past the last.
    virtual void tableAfterEnter(int row);
    /// Insert: add a row.
    virtual bool tableAddRow(int row);
    /// Ctrl+double-click: the table's own properties.
    virtual void tableProperties(int row);
    /// The tooltip of a part: a cell (\p line, \p column), a header cell
    /// (\p column), a toolbar button (\p column is 0 for + and 1 for x) or the
    /// footer. Empty for none.
    virtual QString tableToolTip(int row, TablePart part, int line, int column) const;
    /// Whether a key starts an edit of the current cell: Enter or F2, or a
    /// typed character (\p text), which replaces the cell's text.
    virtual bool tableKeyStartsEdit(int row, int key, const QString &text) const;
    /// The text an editor of a cell opens with (the cell as typed, not as
    /// shown); the shown text when the face has no other.
    virtual QString tableEditText(int row, int line, int column) const;
    /// The cell a right-click is on, line -1 for the header, -2 outside the
    /// cells: what a menu about the cell reads.
    virtual void tableContextAt(int row, int line, int column);
    /// \}
};

inline QString TableFaceSource::tableNickname(int) const
{
    return QString();
}
inline QString TableFaceSource::tableCaption(int) const
{
    return QString();
}
inline bool TableFaceSource::tableToolbar(int) const
{
    return false;
}
inline QString TableFaceSource::tableFooterLeft(int) const
{
    return QString();
}
inline QString TableFaceSource::tableFooterRight(int) const
{
    return QString();
}
inline QColor TableFaceSource::tableFooterColour(int) const
{
    return QColor();
}
inline QString TableFaceSource::tableSummary(int) const
{
    return QString();
}
inline int TableFaceSource::tableSortColumn(int) const
{
    return -1;
}
inline int TableFaceSource::tableSortOrder(int) const
{
    return 0;
}
inline void TableFaceSource::tableSelect(int, int, int, Qt::KeyboardModifiers, bool) {}
inline void TableFaceSource::tableHeaderClicked(int, int) {}
inline void TableFaceSource::tableResizeColumn(int, int, int, bool) {}
inline bool TableFaceSource::tableToolbarClick(int, int, QMenu &)
{
    return false;
}
inline QString TableFaceSource::tableSelectionText(int) const
{
    return QString();
}
inline bool TableFaceSource::tableCurrent(int, int *, int *) const
{
    return false;
}
inline bool TableFaceSource::tableMoveCurrent(int, int, int, bool)
{
    return false;
}
inline void TableFaceSource::tableAfterEnter(int) {}
inline bool TableFaceSource::tableAddRow(int)
{
    return false;
}
inline void TableFaceSource::tableProperties(int) {}
inline QString TableFaceSource::tableToolTip(int, TablePart, int, int) const
{
    return QString();
}
inline bool TableFaceSource::tableKeyStartsEdit(int, int, const QString &) const
{
    return false;
}
inline QString TableFaceSource::tableEditText(int, int, int) const
{
    return QString();
}
inline void TableFaceSource::tableContextAt(int, int, int) {}

/// \brief The spreadsheet behind a face source; null for a plain preview.
inline const TableFaceSource *tableFaceOf(const NodeFaceSource &face)
{
    return dynamic_cast<const TableFaceSource *>(&face);
}

/// \brief The spreadsheet behind a face source; null for a plain preview.
inline TableFaceSource *tableFaceOf(NodeFaceSource &face)
{
    return dynamic_cast<TableFaceSource *>(&face);
}

/// \brief Where the parts of a spreadsheet Table row are, in body
///        coordinates; an empty rectangle for a part that is not shown.
struct TablePieces
{
    QRect nickname;
    QRect caption;
    QRect toolbar;
    /// The header and the lines: the rectangle tableGeometry() takes.
    QRect table;
    /// The scroll bar's column, beside the lines, when there are more lines
    /// than fit; empty otherwise. The table is that much narrower.
    QRect scroll;
    QRect footer;
};

/// The width of a spreadsheet's scroll bar, as the widget's style sheet made it.
inline constexpr int kTableScrollWidth = 8;

/// \brief The thumb of a scroll bar.
/// \param track The bar's track (TablePieces::scroll).
/// \param visible Lines that fit.
/// \param total Lines the table has.
/// \param first The first line shown.
inline QRect tableScrollThumb(const QRect &track, int visible, int total, int first)
{
    if (track.isEmpty() || (total <= 0) || (visible >= total)) {
        return QRect();
    }
    const int thumb = std::clamp((track.height() * visible) / total,
                                 std::min(16, track.height()),
                                 track.height());
    const int travel = track.height() - thumb;
    const int span = std::max(1, total - visible);
    const int at = (travel * std::clamp(first, 0, span)) / span;
    return QRect(track.left(), track.top() + at, track.width(), thumb);
}

/// \brief The metrics of a spreadsheet's table: a line is as high as the
///        table's font needs, never less than the face's label height.
inline FaceMetrics tableMetrics(const NodeFaceSource &face, int row, const FaceMetrics &base)
{
    FaceMetrics metrics = base;
    const TableFaceSource *ext = tableFaceOf(face);
    if (ext != nullptr) {
        const TableLook look = ext->tableLook(row);
        if (look.hasFont) {
            const QFontMetrics fm(look.font);
            metrics.labelHeight = std::max(base.labelHeight, fm.height() + 6);
        }
    }
    return metrics;
}

/// \brief The parts of the row's control.
/// \param control The row's control rectangle.
/// \param metrics tableMetrics() of the row.
inline TablePieces tablePieces(const NodeFaceSource &face,
                               int row,
                               const QRect &control,
                               const FaceMetrics &metrics)
{
    TablePieces pieces;
    const TableFaceSource *ext = tableFaceOf(face);
    if (ext == nullptr) {
        pieces.table = control;
        return pieces;
    }
    const int line = metrics.labelHeight;
    int top = control.top();
    const auto strip = [&](bool shown) {
        QRect rect;
        if (shown && ((top + line) <= (control.bottom() + 1))) {
            rect = QRect(control.left(), top, control.width(), line);
            top += line;
        }
        return rect;
    };
    pieces.nickname = strip(!ext->tableNickname(row).isEmpty());
    pieces.caption = strip(!ext->tableCaption(row).isEmpty());
    pieces.toolbar = strip(ext->tableToolbar(row));
    const int bottom = control.bottom() + 1;
    const int footerTop = std::max(top, bottom - line);
    pieces.footer = QRect(control.left(),
                          footerTop,
                          control.width(),
                          std::max(0, bottom - footerTop));
    QRect table(control.left(), top, control.width(), std::max(0, footerTop - top));
    // More lines than fit: a scroll bar takes the table's right edge.
    const int visible = std::max(0, (table.height() - line) / line);
    if ((visible > 0) && (face.tableLines(row) > visible)) {
        pieces.scroll = QRect(table.right() - kTableScrollWidth + 1,
                              table.top() + line,
                              kTableScrollWidth,
                              std::max(0, table.height() - line));
        table.setWidth(table.width() - kTableScrollWidth);
    }
    pieces.table = table;
    return pieces;
}

/// \brief The button of the toolbar at \p point: 0 for +, 1 for x, -1 for
///        none. Each button is a square as high as the strip.
inline int tableToolbarButtonAt(const TablePieces &pieces, const QPoint &point)
{
    if (!pieces.toolbar.contains(point)) {
        return -1;
    }
    const int side = pieces.toolbar.height();
    const int index = (point.x() - pieces.toolbar.left()) / std::max(1, side);
    return (index < 2) ? index : -1;
}

} // namespace QtNodes
