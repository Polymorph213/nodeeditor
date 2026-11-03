// CanvasConfig.hpp - what the application tells the canvas (CICADA, ADR-1631).
//
// The fork has no knowledge of the environment: the application reads its
// switch (CICADA_CANVAS, ADR-1640) and fills this struct before it builds a
// scene. A scene reads the config once, in its constructor.
#pragma once

#include "Export.hpp"

#include <cstdint>

namespace QtNodes {

/// \brief Which nodes and wires of the model a scene makes items for.
enum class ItemPolicy : std::uint8_t {
  /// Every node and wire has an item, made in the scene's constructor and on
  /// every creation. A bare `DataFlowGraphicsScene(model)` is this: the
  /// default, so a test that asks a scene for any node's item still works.
  All = 0,
  /// No node and no wire has an item. The scene is a selection and a group
  /// table over the model (a test, and the base of the virtual canvas).
  None = 1,
};

/// \brief The canvas settings the application chooses.
struct CanvasConfig {
  /// A node paints its body from rows (NodeFaceSource) and owns no QWidget;
  /// its size comes from the face layout, never from a widget.
  bool painted = false;
  /// Which nodes and wires get an item.
  ItemPolicy itemPolicy = ItemPolicy::All;
};

/// \brief The settings in force.
/// \return The config; default constructed until setCanvasConfig() is called.
NODE_EDITOR_PUBLIC const CanvasConfig &canvasConfig();

/// \brief Replace the settings. Call before a scene is built: a scene reads
///        them once.
/// \param config The new settings.
NODE_EDITOR_PUBLIC void setCanvasConfig(const CanvasConfig &config);

} // namespace QtNodes
