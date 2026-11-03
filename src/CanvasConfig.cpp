// CanvasConfig.cpp - the one process-wide canvas setting (CICADA, ADR-1631).
#include "CanvasConfig.hpp"

namespace QtNodes {

namespace {

/// The settings in force. Written on the GUI thread before a scene is built.
CanvasConfig &storage() {
  static CanvasConfig config;
  return config;
}

} // namespace

const CanvasConfig &canvasConfig() { return storage(); }

void setCanvasConfig(const CanvasConfig &config) { storage() = config; }

} // namespace QtNodes
