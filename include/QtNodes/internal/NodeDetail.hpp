// NodeDetail.hpp - how much of a node and of a wire is drawn at a view scale
// (CICADA, ADR-1882).
//
// The node editor has no zoom-out limit (ADR-1871): at the smallest scales
// every node of a document of ten thousand is on screen, a few pixels wide. A
// node's captions, rows, icons and port labels are unreadable there and cost
// most of the frame, so below a scale a node paints a reduced face (its frame
// and colour) and a wire a straight line. The item's shape and bounding
// rectangle do not change, so hit testing, selection and dragging work at any
// scale.
#pragma once

#include <cstdint>

namespace QtNodes {

/// \brief How much of a node or a wire the painter draws.
enum class NodeDetail : std::uint8_t {
  /// Everything: gradient, port dots, captions, rows, icons.
  Full = 0,
  /// Frame and colour only; a wire is a straight line.
  Reduced = 1,
  /// A wire only: nothing is drawn, the wire is a pixel or two long.
  Hidden = 2,
};

/// The view scale (an item's level of detail) below which the detail is
/// reduced. A caption is about 12 px high at scale 1, so under 0.4 its letters
/// are under 5 px: a smear. The old zoom-out limit was 0.3 (ADR-1871 removed
/// it), and the zoom steps are 1.2 apart, so 0.4 is reached from 1.0 after
/// five steps out.
inline constexpr double kReducedDetailBelow = 0.4;

/// A wire is not drawn under this fraction of the reduced-detail scale
/// (0.075 x 0.4 = 0.03): a wire is a few hundred scene units long, so under
/// 0.03 it is a few pixels, and fifteen thousand of them are most of a frame
/// of ten thousand nodes. A selected wire is still drawn.
inline constexpr double kWiresHiddenFraction = 0.075;

/// A node smaller than this many device pixels across is filled with one
/// colour, without a border pen.
inline constexpr double kFramedNodeMinPixels = 6.0;

/// \brief The detail a node and its wires have at a view scale.
/// \param scale The level of detail of the item: the view's scale, as
///        QStyleOptionGraphicsItem::levelOfDetailFromTransform() gives it.
/// \param below The scale under which the detail is reduced; 0 or less never
///        reduces it (CanvasConfig::reducedDetailBelow).
/// \return Reduced when scale is under \p below, else Full.
constexpr NodeDetail
detailForScale(double scale, double below = kReducedDetailBelow) noexcept {
  return (scale < below) ? NodeDetail::Reduced : NodeDetail::Full;
}

/// \brief The detail a wire has at a view scale.
/// \param scale The level of detail of the item.
/// \param below The scale under which the detail is reduced; 0 or less never
///        reduces it.
/// \return Full at or above \p below, Hidden under kWiresHiddenFraction of it,
///         else Reduced.
constexpr NodeDetail
wireDetailForScale(double scale, double below = kReducedDetailBelow) noexcept {
  if (scale >= below) {
    return NodeDetail::Full;
  }
  return (scale < below * kWiresHiddenFraction) ? NodeDetail::Hidden
                                                : NodeDetail::Reduced;
}

} // namespace QtNodes
