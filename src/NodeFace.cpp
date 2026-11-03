// NodeFace.cpp - the base of a node's face and the placeholder (CICADA,
// ADR-1631).
#include "NodeFace.hpp"

#include "DataFlowGraphModel.hpp"
#include "FaceLayout.hpp"
#include "NodeDelegateModel.hpp"
#include "NodeFaceEditorHost.hpp"

#include <QtCore/QList>

#include <memory>

namespace QtNodes {

namespace {

/// A face of one fixed Label row: what a node paints before it has rows.
class PlaceholderFace final : public NodeFaceSource {
public:
  PlaceholderFace()
      : rows_(std::make_shared<const std::vector<RowSpec>>(makeRows())) {}

  std::shared_ptr<const std::vector<RowSpec>> rows() const override {
    return rows_;
  }
  QVariant value(int row) const override {
    static_cast<void>(row);
    return QVariant();
  }
  bool setValue(int row, const QVariant &value, EditSource source) override {
    static_cast<void>(row);
    static_cast<void>(value);
    static_cast<void>(source);
    return false;
  }
  std::uint64_t revision() const override { return 1U; }
  bool rowEnabled(int row) const override {
    static_cast<void>(row);
    return false;
  }

private:
  static std::vector<RowSpec> makeRows() {
    RowSpec spec;
    spec.kind = RowKind::Label;
    spec.id = QStringLiteral("notConverted");
    spec.label = QStringLiteral("(not converted)");
    spec.flags = kRowDisabled;
    std::vector<RowSpec> rows;
    rows.push_back(spec);
    return rows;
  }

  std::shared_ptr<const std::vector<RowSpec>> rows_;
};

} // namespace

NodeFaceEditorHost::~NodeFaceEditorHost() = default;

NodeFaceSource::NodeFaceSource() = default;

NodeFaceSource::~NodeFaceSource() = default;

bool NodeFaceSource::rowVisible(int row) const {
  static_cast<void>(row);
  return true;
}

bool NodeFaceSource::rowEnabled(int row) const {
  const std::shared_ptr<const std::vector<RowSpec>> all = rows();
  if (!all || (row < 0) || (static_cast<std::size_t>(row) >= all->size())) {
    return false;
  }
  return ((*all)[static_cast<std::size_t>(row)].flags & kRowDisabled) == 0U;
}

bool NodeFaceSource::invoke(int row) {
  static_cast<void>(row);
  return false;
}

QStringList NodeFaceSource::choicesOf(int row) const {
  const std::shared_ptr<const std::vector<RowSpec>> all = rows();
  if (all && (row >= 0) && (static_cast<std::size_t>(row) < all->size())) {
    return (*all)[static_cast<std::size_t>(row)].choices;
  }
  return QStringList();
}

QStringList NodeFaceSource::choiceLabelsOf(int row) const {
  const std::shared_ptr<const std::vector<RowSpec>> all = rows();
  if (all && (row >= 0) && (static_cast<std::size_t>(row) < all->size())) {
    return (*all)[static_cast<std::size_t>(row)].choiceLabels;
  }
  return QStringList();
}

QSize NodeFaceSource::minBodySize() const { return QSize(); }

bool NodeFaceSource::exactBodyWidth() const { return false; }

std::vector<FaceFrame> NodeFaceSource::frames() const { return {}; }

int NodeFaceSource::bodyMargin() const { return -1; }

int NodeFaceSource::rowGap() const { return -1; }

RowDisplay NodeFaceSource::display(int row) const {
  RowDisplay shown;
  const std::shared_ptr<const std::vector<RowSpec>> all = rows();
  if (all && (row >= 0) && (static_cast<std::size_t>(row) < all->size())) {
    shown.suffix = (*all)[static_cast<std::size_t>(row)].units;
  }
  return shown;
}

QString NodeFaceSource::displayText(int row, int component) const {
  const std::shared_ptr<const std::vector<RowSpec>> all = rows();
  if (!all || (row < 0) || (static_cast<std::size_t>(row) >= all->size())) {
    return QString();
  }
  const RowSpec &spec = (*all)[static_cast<std::size_t>(row)];
  const QVariant v = value(row);
  const RowDisplay shown = display(row);
  const QString suffix =
      shown.suffix.isEmpty() ? QString() : (QStringLiteral(" ") + shown.suffix);
  switch (spec.kind) {
  case RowKind::Int:
    return QString::number(v.toInt()) + suffix;
  case RowKind::Double:
  case RowKind::Slider:
    return QString::number(v.toDouble() * shown.scale, 'g', 6) + suffix;
  case RowKind::Enum: {
    const QStringList ids = choicesOf(row);
    const QStringList words = choiceLabelsOf(row);
    const int at = ids.indexOf(v.toString());
    if ((at >= 0) && (at < words.size())) {
      return words.at(at);
    }
    return v.toString();
  }
  case RowKind::String:
    return v.toString();
  case RowKind::Vec3: {
    const QVariantList parts = v.toList();
    if ((component >= 0) && (component < parts.size())) {
      return QString::number(parts.at(component).toDouble(), 'g', 6);
    }
    return QString();
  }
  case RowKind::Label:
  case RowKind::Button:
  case RowKind::Section:
    return spec.label;
  case RowKind::Bool:
  case RowKind::Swatch:
  case RowKind::Table:
  case RowKind::PortAlign:
  default:
    return QString();
  }
}

QString NodeFaceSource::refusalOf(int row) const {
  static_cast<void>(row);
  return QString();
}

int NodeFaceSource::tableColumns(int row) const {
  static_cast<void>(row);
  return 0;
}

int NodeFaceSource::tableLines(int row) const {
  static_cast<void>(row);
  return 0;
}

int NodeFaceSource::tableFirstLine(int row) const {
  static_cast<void>(row);
  return 0;
}

QString NodeFaceSource::tableHeader(int row, int column) const {
  static_cast<void>(row);
  static_cast<void>(column);
  return QString();
}

QString NodeFaceSource::tableCell(int row, int line, int column) const {
  static_cast<void>(row);
  static_cast<void>(line);
  static_cast<void>(column);
  return QString();
}

int NodeFaceSource::tableColumnWidth(int row, int column) const {
  static_cast<void>(row);
  static_cast<void>(column);
  return 0;
}

bool NodeFaceSource::tableCellEditable(int row, int line, int column) const {
  static_cast<void>(row);
  static_cast<void>(line);
  static_cast<void>(column);
  return false;
}

bool NodeFaceSource::setTableCell(int row, int line, int column,
                                  const QString &text, EditSource source) {
  static_cast<void>(row);
  static_cast<void>(line);
  static_cast<void>(column);
  static_cast<void>(text);
  static_cast<void>(source);
  return false;
}

bool NodeFaceSource::scrollTable(int row, int lines) {
  static_cast<void>(row);
  static_cast<void>(lines);
  return false;
}

bool NodeFaceSource::resizeBody(const QSize &size, bool final) {
  static_cast<void>(size);
  static_cast<void>(final);
  return false;
}

bool NodeFaceSource::alignsToPorts() const { return false; }

bool NodeFaceSource::sectionExpanded(int section) const {
  constexpr int kMaxSections = 64;
  if (section <= 0) {
    return true;
  }
  const bool startsOpen = (section == 1);
  if (section > kMaxSections) {
    return startsOpen;
  }
  const std::uint64_t bit = std::uint64_t{1} << (section - 1);
  const bool toggled = (sectionToggles_ & bit) != 0U;
  return toggled ? !startsOpen : startsOpen;
}

void NodeFaceSource::toggleSection(int section) {
  constexpr int kMaxSections = 64;
  if ((section <= 0) || (section > kMaxSections)) {
    return;
  }
  sectionToggles_ ^= (std::uint64_t{1} << (section - 1));
}

std::unique_ptr<FaceLayoutResult> &NodeFaceSource::layoutSlot() const {
  return layout_;
}

NodeFaceSource &NodeFaceSource::placeholder() {
  static PlaceholderFace kFace;
  return kFace;
}

NodeFaceSource &faceOf(AbstractGraphModel &model, NodeId node) {
  if (auto *flow = dynamic_cast<DataFlowGraphModel *>(&model)) {
    if (auto *delegate = flow->delegateModel<NodeDelegateModel>(node)) {
      if (NodeFaceSource *face = delegate->faceSource()) {
        return *face;
      }
    }
  }
  return NodeFaceSource::placeholder();
}

} // namespace QtNodes
