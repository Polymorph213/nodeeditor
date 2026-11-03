#include "BasicGraphicsScene.hpp"

#include "AbstractNodeGeometry.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "DataFlowGraphModel.hpp"
#include "DefaultConnectionPainter.hpp"
#include "DefaultHorizontalNodeGeometry.hpp"
#include "DefaultNodePainter.hpp"
#include "DefaultVerticalNodeGeometry.hpp"
#include "GraphicsView.hpp"
#include "NodeDelegateModel.hpp"
#include "NodeGraphicsObject.hpp"
#include "cicada/util/CommandTrace.h" // CICADA: command trace (ADR-1495)

#include <QAccessible> // CICADA: quiet the platform bridge while a node is built
#include <QColorDialog> // CICADA group polish: pick group fill color
#include <QUndoStack>

#include <QHeaderView>
#include <QLineEdit>
#include <QTreeWidget>
#include <QWidgetAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsSceneMoveEvent>

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <queue>

namespace {

using QtNodes::GroupId;
using QtNodes::InvalidGroupId;
using QtNodes::InvalidNodeId;
using QtNodes::NodeId;

/// CICADA_LEGACY_NODE_PATHS=1 puts the node graphics paths below back as they
/// were before ADR-1468, so one build can be timed against itself.
bool legacyNodePaths()
{
    static bool const legacy = qEnvironmentVariableIsSet("CICADA_LEGACY_NODE_PATHS");
    return legacy;
}

/// While alive, accessibility events are dropped instead of being handed to the
/// platform's accessibility bridge (ADR-1468).
///
/// A node's widgets are built inside its graphics object: every QLineEdit
/// setText and QSpinBox setValue of its editor, and the proxy widget's show.
/// With a UI Automation client listening, each of those is a call to
/// UiaRaiseAutomationEvent about a widget no client has seen yet, and it is
/// answered across a process boundary. In a sampled Polyline that was 28 % of
/// the GUI thread's working time.
class AccessibilityEventsPaused
{
public:
    AccessibilityEventsPaused()
    {
#if QT_CONFIG(accessibility)
        if (!legacyNodePaths()) {
            _previous = QAccessible::installUpdateHandler(&AccessibilityEventsPaused::drop);
            _installed = true;
        }
#endif
    }

    ~AccessibilityEventsPaused()
    {
#if QT_CONFIG(accessibility)
        if (_installed)
            QAccessible::installUpdateHandler(_previous);
#endif
    }

    AccessibilityEventsPaused(AccessibilityEventsPaused const &) = delete;
    AccessibilityEventsPaused &operator=(AccessibilityEventsPaused const &) = delete;

private:
#if QT_CONFIG(accessibility)
    static void drop(QAccessibleEvent *) {}

    QAccessible::UpdateHandler _previous = nullptr;
    bool _installed = false;
#endif
};

NodeId jsonValueToNodeId(QJsonValue const &value)
{
    if (value.isDouble()) {
        return static_cast<NodeId>(value.toInt());
    }

    if (value.isString()) {
        auto const textValue = value.toString();

        bool ok = false;
        auto const numericValue = textValue.toULongLong(&ok, 10);
        if (ok) {
            return static_cast<NodeId>(numericValue);
        }

        QUuid uuidValue(textValue);
        if (!uuidValue.isNull()) {
            auto const bytes = uuidValue.toRfc4122();
            if (bytes.size() >= static_cast<int>(sizeof(quint32))) {
                QDataStream stream(bytes);
                quint32 value32 = 0U;
                stream >> value32;
                return static_cast<NodeId>(value32);
            }
        }
    }

    return InvalidNodeId;
}

} // namespace

namespace QtNodes {

BasicGraphicsScene::BasicGraphicsScene(AbstractGraphModel &graphModel, QObject *parent)
    : QGraphicsScene(parent)
    , _graphModel(graphModel)
    , _nodeGeometry(std::make_unique<DefaultHorizontalNodeGeometry>(_graphModel))
    , _nodePainter(std::make_unique<DefaultNodePainter>())
    , _connectionPainter(std::make_unique<DefaultConnectionPainter>())
    , _nodeDrag(false)
    , _undoStack(new QUndoStack(this))
    , _orientation(Qt::Horizontal)
    , _groupingEnabled(true)
{
    setItemIndexMethod(QGraphicsScene::NoIndex);

    connect(&_graphModel,
            &AbstractGraphModel::connectionCreated,
            this,
            &BasicGraphicsScene::onConnectionCreated);

    connect(&_graphModel,
            &AbstractGraphModel::connectionDeleted,
            this,
            &BasicGraphicsScene::onConnectionDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::nodeCreated,
            this,
            &BasicGraphicsScene::onNodeCreated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeDeleted,
            this,
            &BasicGraphicsScene::onNodeDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::nodePositionUpdated,
            this,
            &BasicGraphicsScene::onNodePositionUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeUpdated,
            this,
            &BasicGraphicsScene::onNodeUpdated);

    connect(this, &BasicGraphicsScene::nodeClicked, this, &BasicGraphicsScene::onNodeClicked);

    connect(&_graphModel, &AbstractGraphModel::modelReset, this, &BasicGraphicsScene::onModelReset);

    // CICADA (ADR-1638): which nodes and wires get an item is the
    // application's choice, read once. A bare scene is policy All.
    _itemPolicy = canvasConfig().itemPolicy;

    traverseGraphAndPopulateGraphicsObjects();
}

BasicGraphicsScene::~BasicGraphicsScene() = default;

AbstractGraphModel const &BasicGraphicsScene::graphModel() const
{
    return _graphModel;
}

AbstractGraphModel &BasicGraphicsScene::graphModel()
{
    return _graphModel;
}

AbstractNodeGeometry const &BasicGraphicsScene::nodeGeometry() const
{
    return *_nodeGeometry;
}

AbstractNodeGeometry &BasicGraphicsScene::nodeGeometry()
{
    return *_nodeGeometry;
}

AbstractNodePainter &BasicGraphicsScene::nodePainter()
{
    return *_nodePainter;
}

AbstractConnectionPainter &BasicGraphicsScene::connectionPainter()
{
    return *_connectionPainter;
}

void BasicGraphicsScene::setNodePainter(std::unique_ptr<AbstractNodePainter> newPainter)
{
    _nodePainter = std::move(newPainter);
}

void BasicGraphicsScene::setConnectionPainter(std::unique_ptr<AbstractConnectionPainter> newPainter)
{
    _connectionPainter = std::move(newPainter);
}

void BasicGraphicsScene::setNodeGeometry(std::unique_ptr<AbstractNodeGeometry> newGeom)
{
    _nodeGeometry = std::move(newGeom);
}

QUndoStack &BasicGraphicsScene::undoStack()
{
    return *_undoStack;
}

void BasicGraphicsScene::setGroupingEnabled(bool enabled)
{
    if (_groupingEnabled == enabled)
        return;

    if (!enabled) {
        for (auto &groupEntry : _groups) {
            auto &group = groupEntry.second;
            if (!group)
                continue;

            for (auto *node : group->childNodes()) {
                if (!node)
                    continue;

                node->unsetNodeGroup();
                node->lock(false);
            }
        }

        _groups.clear();
        _groupOfNode.clear();
        _selectedGroups.clear();
        _nextGroupId = 0;
    }

    _groupingEnabled = enabled;
}

std::unique_ptr<ConnectionGraphicsObject> const &BasicGraphicsScene::makeDraftConnection(
    ConnectionId const incompleteConnectionId)
{
    _draftConnection = std::make_unique<ConnectionGraphicsObject>(*this, incompleteConnectionId);

    _draftConnection->grabMouse();

    return _draftConnection;
}

void BasicGraphicsScene::resetDraftConnection()
{
    _draftConnection.reset();
}

void BasicGraphicsScene::clearScene()
{
    auto const &allNodeIds = graphModel().allNodeIds();

    for (auto nodeId : allNodeIds) {
        graphModel().deleteNode(nodeId);
    }
}

std::vector<std::shared_ptr<ConnectionId>> BasicGraphicsScene::connectionsWithinGroup(GroupId groupID)
{
    if (!_groupingEnabled)
        return {};

    std::vector<std::shared_ptr<ConnectionId>> ret{};

    // CICADA (ADR-1638): from the model and the group table, never from the
    // items that exist: a wire both of whose ends are members of the group.
    auto const groupIt = _groups.find(groupID);
    if (groupIt == _groups.end())
        return ret;

    for (NodeId const member : groupIt->second->nodeIDs()) {
        for (ConnectionId const &cid : _graphModel.allConnectionIds(member)) {
            if (cid.outNodeId != member)
                continue; // each wire once, from its output end
            if (groupOfNode(cid.inNodeId) == groupID)
                ret.push_back(std::make_shared<ConnectionId>(cid));
        }
    }

    return ret;
}

NodeGraphicsObject *BasicGraphicsScene::ensureNodeItem(NodeId nodeId)
{
    if (NodeGraphicsObject *existing = nodeGraphicsObject(nodeId))
        return existing;
    if (!_graphModel.nodeExists(nodeId))
        return nullptr;
    AccessibilityEventsPaused const quiet;
    auto item = std::make_unique<NodeGraphicsObject>(*this, nodeId);
    NodeGraphicsObject *const raw = item.get();
    _nodeGraphicsObjects[nodeId] = std::move(item);
    if (auto const group = groupOfNode(nodeId); group != InvalidGroupId) {
        auto const it = _groups.find(group);
        if (it != _groups.end())
            raw->setNodeGroup(it->second);
    }
    if (_selectedNodes.count(nodeId) != 0)
        raw->setSelected(true);
    return raw;
}

ConnectionGraphicsObject *BasicGraphicsScene::ensureConnectionItem(ConnectionId connectionId)
{
    if (ConnectionGraphicsObject *existing = connectionGraphicsObject(connectionId))
        return existing;
    if (!_graphModel.connectionExists(connectionId))
        return nullptr;
    auto item = std::make_unique<ConnectionGraphicsObject>(*this, connectionId);
    ConnectionGraphicsObject *const raw = item.get();
    _connectionGraphicsObjects[connectionId] = std::move(item);
    if (_selectedConnections.count(connectionId) != 0)
        raw->setSelected(true);
    return raw;
}

std::vector<NodeId> BasicGraphicsScene::selectedNodeIds() const
{
    std::vector<NodeId> ids(_selectedNodes.begin(), _selectedNodes.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<ConnectionId> BasicGraphicsScene::selectedConnectionIds() const
{
    return std::vector<ConnectionId>(_selectedConnections.begin(), _selectedConnections.end());
}

std::vector<GroupId> BasicGraphicsScene::selectedGroupIds() const
{
    std::vector<GroupId> ids(_selectedGroups.begin(), _selectedGroups.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool BasicGraphicsScene::isNodeSelected(NodeId nodeId) const
{
    return _selectedNodes.count(nodeId) != 0;
}

void BasicGraphicsScene::selectNode(NodeId nodeId, bool selected)
{
    if (selected) {
        if (!_graphModel.nodeExists(nodeId))
            return;
        _selectedNodes.insert(nodeId);
    } else {
        _selectedNodes.erase(nodeId);
    }
    if (NodeGraphicsObject *item = nodeGraphicsObject(nodeId)) {
        if (item->isSelected() != selected)
            item->setSelected(selected);
    }
}

void BasicGraphicsScene::selectConnection(ConnectionId connectionId, bool selected)
{
    if (selected) {
        if (!_graphModel.connectionExists(connectionId))
            return;
        _selectedConnections.insert(connectionId);
    } else {
        _selectedConnections.erase(connectionId);
    }
    if (ConnectionGraphicsObject *item = connectionGraphicsObject(connectionId)) {
        if (item->isSelected() != selected)
            item->setSelected(selected);
    }
}

void BasicGraphicsScene::selectAllNodes()
{
    for (NodeId const nodeId : _graphModel.allNodeIds())
        selectNode(nodeId, true);
}

void BasicGraphicsScene::clearAllSelection()
{
    // The items first: each reports its own deselection through itemChange.
    clearSelection();
    _selectedNodes.clear();
    _selectedConnections.clear();
    _selectedGroups.clear();
}

void BasicGraphicsScene::noteNodeSelection(NodeId nodeId, bool selected)
{
    if (selected)
        _selectedNodes.insert(nodeId);
    else
        _selectedNodes.erase(nodeId);
}

void BasicGraphicsScene::noteConnectionSelection(ConnectionId connectionId, bool selected)
{
    if (selected)
        _selectedConnections.insert(connectionId);
    else
        _selectedConnections.erase(connectionId);
}

void BasicGraphicsScene::noteGroupSelection(GroupId groupId, bool selected)
{
    if (selected)
        _selectedGroups.insert(groupId);
    else
        _selectedGroups.erase(groupId);
}

GroupId BasicGraphicsScene::groupOfNode(NodeId nodeId) const
{
    auto const it = _groupOfNode.find(nodeId);
    return it == _groupOfNode.end() ? InvalidGroupId : it->second;
}

NodeGraphicsObject *BasicGraphicsScene::nodeGraphicsObject(NodeId nodeId)
{
    NodeGraphicsObject *ngo = nullptr;
    auto it = _nodeGraphicsObjects.find(nodeId);
    if (it != _nodeGraphicsObjects.end()) {
        ngo = it->second.get();
    }

    return ngo;
}

ConnectionGraphicsObject *BasicGraphicsScene::connectionGraphicsObject(ConnectionId connectionId)
{
    ConnectionGraphicsObject *cgo = nullptr;
    auto it = _connectionGraphicsObjects.find(connectionId);
    if (it != _connectionGraphicsObjects.end()) {
        cgo = it->second.get();
    }

    return cgo;
}

void BasicGraphicsScene::setOrientation(Qt::Orientation const orientation)
{
    if (_orientation != orientation) {
        _orientation = orientation;

        switch (_orientation) {
        case Qt::Horizontal:
            _nodeGeometry = std::make_unique<DefaultHorizontalNodeGeometry>(_graphModel);
            break;

        case Qt::Vertical:
            _nodeGeometry = std::make_unique<DefaultVerticalNodeGeometry>(_graphModel);
            break;
        }

        onModelReset();
    }
}

QMenu *BasicGraphicsScene::createSceneMenu(QPointF const scenePos)
{
    Q_UNUSED(scenePos);
    return nullptr;
}

void BasicGraphicsScene::traverseGraphAndPopulateGraphicsObjects()
{
    // CICADA (ADR-1638): policy None makes no item, for nodes or for wires.
    if (_itemPolicy == ItemPolicy::None)
        return;

    auto allNodeIds = _graphModel.allNodeIds();

    // First create all the nodes. Their widgets are built here, which is not
    // worth an accessibility event each (see AccessibilityEventsPaused).
    AccessibilityEventsPaused const quiet;
    for (NodeId const nodeId : allNodeIds) {
        _nodeGraphicsObjects[nodeId] = std::make_unique<NodeGraphicsObject>(*this, nodeId);
    }

    // Then for each node check output connections and insert them.
    for (NodeId const nodeId : allNodeIds) {
        auto nOutPorts = _graphModel.nodeData<PortCount>(nodeId, NodeRole::OutPortCount);

        for (PortIndex index = 0; index < nOutPorts; ++index) {
            auto const &outConnectionIds = _graphModel.connections(nodeId, PortType::Out, index);

            for (auto cid : outConnectionIds) {
                _connectionGraphicsObjects[cid] = std::make_unique<ConnectionGraphicsObject>(*this,
                                                                                             cid);
            }
        }
    }
}

void BasicGraphicsScene::updateAttachedNodes(ConnectionId const connectionId,
                                             PortType const portType)
{
    auto node = nodeGraphicsObject(getNodeId(portType, connectionId));

    if (node) {
        node->update();
    }
}

void BasicGraphicsScene::onConnectionDeleted(ConnectionId const connectionId)
{
    auto it = _connectionGraphicsObjects.find(connectionId);
    if (it != _connectionGraphicsObjects.end()) {
        _connectionGraphicsObjects.erase(it);
    }
    _selectedConnections.erase(connectionId);

    // TODO: do we need it?
    if (_draftConnection && _draftConnection->connectionId() == connectionId) {
        _draftConnection.reset();
    }

    updateAttachedNodes(connectionId, PortType::Out);
    updateAttachedNodes(connectionId, PortType::In);

    Q_EMIT modified(this);
}

void BasicGraphicsScene::onConnectionCreated(ConnectionId const connectionId)
{
    CICADA_TRACE_AS(Ui, "QtNodes.sceneConnectionCreated");
    if (_itemPolicy != ItemPolicy::None) {
        _connectionGraphicsObjects[connectionId]
            = std::make_unique<ConnectionGraphicsObject>(*this, connectionId);
    }

    updateAttachedNodes(connectionId, PortType::Out);
    updateAttachedNodes(connectionId, PortType::In);

    Q_EMIT modified(this);
}

void BasicGraphicsScene::onNodeDeleted(NodeId const nodeId)
{
    // CICADA (ADR-1638): the group table and the selection are by id, so a
    // node leaves them whether or not it had an item.
    removeNodeFromGroup(nodeId);
    _selectedNodes.erase(nodeId);

    auto it = _nodeGraphicsObjects.find(nodeId);
    if (it != _nodeGraphicsObjects.end()) {
        _nodeGraphicsObjects.erase(it);

        Q_EMIT modified(this);
    }
}

void BasicGraphicsScene::onNodeCreated(NodeId const nodeId)
{
    CICADA_TRACE_AS(Ui, "QtNodes.sceneNodeCreated");
    if (_itemPolicy == ItemPolicy::None) {
        // No item. The size is still the layout's (the wires that reach the
        // node, and a canvas that opens later, read it).
        if (canvasConfig().painted)
            _nodeGeometry->recomputeSize(nodeId);
        Q_EMIT modified(this);
        return;
    }
    {
        AccessibilityEventsPaused const quiet;
        _nodeGraphicsObjects[nodeId] = std::make_unique<NodeGraphicsObject>(*this, nodeId);
    }

    Q_EMIT modified(this);
}

void BasicGraphicsScene::onNodePositionUpdated(NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);
    if (node) {
        node->setPos(_graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>());
        node->update();
        _nodeDrag = true;
    }
}

bool BasicGraphicsScene::hasVisibleView() const
{
    for (QGraphicsView const *view : views()) {
        if (view->isVisible())
            return true;
    }
    return false;
}

void BasicGraphicsScene::embedPendingWidgets()
{
    for (auto &entry : _nodeGraphicsObjects)
        entry.second->embedPendingWidget();
}

void BasicGraphicsScene::onNodeUpdated(NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);

    // CICADA (ADR-1638): a node with no item still has its size.
    if (!node && canvasConfig().painted && _graphModel.nodeExists(nodeId))
        _nodeGeometry->recomputeSize(nodeId);

    if (node) {
        node->setGeometryChanged();

        QSize const before = _nodeGeometry->size(nodeId);
        _nodeGeometry->recomputeSize(nodeId);

        node->updateQWidgetEmbedPos();
        node->update();

        // CICADA (ADR-1468): a wire ends on a port and a port sits where the
        // node's size and port list put it, so a node whose size did not move
        // (a new tint, a caption that still fits) has not moved its wires.
        // Re-measuring every wire's port for such an update cost the node's
        // degree: the world plane, one wire per object, took 12 % of the GUI
        // thread's working time on a Box at 480 objects.
        if (legacyNodePaths() || _nodeGeometry->size(nodeId) != before)
            node->moveConnections();
    }
}

void BasicGraphicsScene::onNodeClicked(NodeId const nodeId)
{
    if (_nodeDrag) {
        Q_EMIT nodeMoved(nodeId, _graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>());
        Q_EMIT modified(this);
    }
    _nodeDrag = false;
}

void BasicGraphicsScene::onModelReset()
{
    _connectionGraphicsObjects.clear();
    _nodeGraphicsObjects.clear();
    _selectedNodes.clear();
    _selectedConnections.clear();
    _selectedGroups.clear();
    _groupOfNode.clear();

    clear();

    traverseGraphAndPopulateGraphicsObjects();
}

void BasicGraphicsScene::freezeModelAndConnections(bool isFreeze)
{
    // CICADA (ADR-1638): the selected nodes by id, not by the items that exist.
    for (NodeId const nodeId : selectedNodeIds()) {
        int portCount = graphModel().nodeData(nodeId, NodeRole::OutPortCount).toInt();
        for (int i = 0; i < portCount; i++) {
            auto graphConnections = graphModel().connections(nodeId,
                                                             QtNodes::PortType::Out,
                                                             QtNodes::PortIndex(i));

            for (auto const &c : graphConnections) {
                if (auto *cgo = connectionGraphicsObject(c)) {
                    cgo->connectionState().setFrozen(isFreeze);
                    cgo->update();
                }
            }
        }

        if (auto *dfModel = dynamic_cast<DataFlowGraphModel *>(&graphModel())) {
            if (auto *delegate = dfModel->delegateModel<NodeDelegateModel>(nodeId)) {
                delegate->setFrozenState(isFreeze);
            }
        }
    }
}

std::weak_ptr<NodeGroup> BasicGraphicsScene::createGroup(std::vector<NodeGraphicsObject *> &nodes,
                                                         QString groupName,
                                                         GroupId groupId)
{
    std::vector<NodeId> ids;
    ids.reserve(nodes.size());
    for (auto *node : nodes) {
        if (node)
            ids.push_back(node->nodeId());
    }
    return createGroupFromIds(ids, std::move(groupName), groupId);
}

std::weak_ptr<NodeGroup> BasicGraphicsScene::createGroupFromIds(std::vector<NodeId> const &nodes,
                                                                QString groupName,
                                                                GroupId groupId)
{
    if (!_groupingEnabled)
        return std::weak_ptr<NodeGroup>();

    if (nodes.empty())
        return std::weak_ptr<NodeGroup>();

    for (NodeId const nodeId : nodes) {
        if (groupOfNode(nodeId) != InvalidGroupId)
            removeNodeFromGroup(nodeId);
    }

    if (groupName.isEmpty()) {
        groupName = "Group " + QString::number(NodeGroup::groupCount());
    }

    if (groupId == InvalidGroupId) {
        groupId = nextGroupId();
    } else {
        if (_groups.count(groupId) != 0) {
            throw std::runtime_error("Group identifier collision");
        }

        if (groupId >= _nextGroupId && _nextGroupId != InvalidGroupId) {
            _nextGroupId = groupId + 1;
        }
    }

    auto group = std::make_shared<NodeGroup>(nodes, groupId, groupName, this);
    // CICADA (ADR-1638): a scene that makes no items makes no group item.
    if (_itemPolicy != ItemPolicy::None) {
        auto ggo = std::make_unique<GroupGraphicsObject>(*this, *group);
        group->setGraphicsObject(std::move(ggo));
    }

    for (NodeId const nodeId : nodes) {
        _groupOfNode[nodeId] = group->id();
        if (NodeGraphicsObject *node = nodeGraphicsObject(nodeId))
            node->setNodeGroup(group);
    }

    std::weak_ptr<NodeGroup> groupWeakPtr = group;

    _groups[group->id()] = std::move(group);

    return groupWeakPtr;
}

std::vector<NodeGraphicsObject *> BasicGraphicsScene::selectedNodes() const
{
    // CICADA (ADR-1638): the items of the selected nodes that have one. A
    // caller that acts on the selection reads selectedNodeIds().
    std::vector<NodeGraphicsObject *> result;
    result.reserve(_selectedNodes.size());

    for (NodeId const nodeId : selectedNodeIds()) {
        auto const it = _nodeGraphicsObjects.find(nodeId);
        if (it != _nodeGraphicsObjects.end())
            result.push_back(it->second.get());
    }

    return result;
}

std::vector<GroupGraphicsObject *> BasicGraphicsScene::selectedGroups() const
{
    if (!_groupingEnabled)
        return {};

    QList<QGraphicsItem *> graphicsItems = selectedItems();

    std::vector<GroupGraphicsObject *> result;
    result.reserve(graphicsItems.size());

    for (QGraphicsItem *item : graphicsItems) {
        auto ngo = qgraphicsitem_cast<GroupGraphicsObject *>(item);

        if (ngo) {
            result.push_back(ngo);
        }
    }

    return result;
}

void BasicGraphicsScene::addNodeToGroup(NodeId nodeId, GroupId groupId)
{
    if (!_groupingEnabled)
        return;

    // CICADA (ADR-1638): by id; the node needs no item.
    auto groupIt = _groups.find(groupId);
    if (groupIt == _groups.end() || !_graphModel.nodeExists(nodeId))
        return;

    auto group = groupIt->second;
    group->addNodeId(nodeId);
    _groupOfNode[nodeId] = groupId;
    if (NodeGraphicsObject *node = nodeGraphicsObject(nodeId))
        node->setNodeGroup(group);
}

void BasicGraphicsScene::removeNodeFromGroup(NodeId nodeId)
{
    if (!_groupingEnabled)
        return;

    auto const where = _groupOfNode.find(nodeId);
    if (where == _groupOfNode.end())
        return;

    auto const groupIt = _groups.find(where->second);
    _groupOfNode.erase(where);
    if (groupIt != _groups.end()) {
        std::shared_ptr<NodeGroup> const group = groupIt->second;
        group->removeNodeId(nodeId);
        if (group->empty()) {
            _selectedGroups.erase(group->id());
            _groups.erase(group->id());
        }
    }
    if (NodeGraphicsObject *node = nodeGraphicsObject(nodeId)) {
        node->unsetNodeGroup();
        node->lock(false);
    }
}

std::weak_ptr<QtNodes::NodeGroup> BasicGraphicsScene::createGroupFromSelection(QString groupName)
{
    if (!_groupingEnabled)
        return std::weak_ptr<NodeGroup>();

    // CICADA (ADR-1638): the selected nodes by id; a select-all groups every
    // node, not the ones that have an item.
    return createGroupFromIds(selectedNodeIds(), groupName);
}

NodeId BasicGraphicsScene::loadNodeToMap(QJsonObject nodeJson, bool keepOriginalId)
{
    NodeId newNodeId = InvalidNodeId;

    if (keepOriginalId) {
        newNodeId = jsonValueToNodeId(nodeJson["id"]);
    }

    if (newNodeId == InvalidNodeId) {
        newNodeId = _graphModel.newNodeId();
        nodeJson["id"] = static_cast<qint64>(newNodeId);
    }

    _graphModel.loadNode(nodeJson);

    // CICADA (ADR-1638): an item only where the policy makes one.
    if (_itemPolicy != ItemPolicy::None && !nodeGraphicsObject(newNodeId))
        ensureNodeItem(newNodeId);

    return newNodeId;
}

void BasicGraphicsScene::loadConnectionToMap(QJsonObject const &connectionJson,
                                             std::unordered_map<NodeId, NodeId> const &nodeIdMap)
{
    ConnectionId connId = fromJson(connectionJson);

    auto const outIt = nodeIdMap.find(connId.outNodeId);
    auto const inIt = nodeIdMap.find(connId.inNodeId);

    if (outIt == nodeIdMap.end() || inIt == nodeIdMap.end()) {
        return;
    }

    ConnectionId remapped{outIt->second, connId.outPortIndex, inIt->second, connId.inPortIndex};

    if (_graphModel.connectionExists(remapped)) {
        return;
    }

    if (_graphModel.connectionPossible(remapped)) {
        _graphModel.addConnection(remapped);
    }
}

std::pair<std::weak_ptr<NodeGroup>, std::unordered_map<GroupId, GroupId>>
BasicGraphicsScene::restoreGroup(QJsonObject const &groupJson)
{
    if (!_groupingEnabled)
        return {std::weak_ptr<NodeGroup>(), {}};

    // since the new nodes will have the same IDs as in the file and the connections
    // need these old IDs to be restored, we must create new IDs and map them to the
    // old ones so the connections are properly restored
    std::unordered_map<GroupId, GroupId> IDsMap{};
    std::unordered_map<NodeId, NodeId> nodeIdMap{};

    std::vector<NodeId> group_children{};

    QJsonArray nodesJson = groupJson["nodes"].toArray();
    for (const QJsonValueRef nodeJson : nodesJson) {
        QJsonObject nodeObject = nodeJson.toObject();
        NodeId const oldNodeId = jsonValueToNodeId(nodeObject["id"]);

        NodeId const newNodeId = loadNodeToMap(nodeObject, false);

        if (oldNodeId != InvalidNodeId) {
            nodeIdMap.emplace(oldNodeId, newNodeId);
            IDsMap.emplace(static_cast<GroupId>(oldNodeId), static_cast<GroupId>(newNodeId));
        }

        group_children.push_back(newNodeId);
    }

    QJsonArray connectionJsonArray = groupJson["connections"].toArray();
    for (auto connection : connectionJsonArray) {
        loadConnectionToMap(connection.toObject(), nodeIdMap);
    }

    return std::make_pair(createGroupFromIds(group_children, groupJson["name"].toString()), IDsMap);
}

std::unordered_map<GroupId, std::shared_ptr<NodeGroup>> const &BasicGraphicsScene::groups() const
{
    return _groups;
}

QMenu *BasicGraphicsScene::createStdMenu(QPointF const scenePos)
{
    Q_UNUSED(scenePos);
    QMenu *menu = new QMenu();

    if (_groupingEnabled) {
        QMenu *addToGroupMenu = menu->addMenu("Add to group...");

        for (const auto &groupMap : _groups) {
            auto groupPtr = groupMap.second;
            auto id = groupMap.first;

            if (!groupPtr)
                continue;

            auto groupName = groupPtr->name();

            QAction *groupAction = addToGroupMenu->addAction(groupName);

            for (const auto &node : selectedNodes()) {
                connect(groupAction, &QAction::triggered, [this, id, node]() {
                    this->addNodeToGroup(node->nodeId(), id);
                });
            }
        }

        QAction *createGroupAction = menu->addAction("Create group from selection");
        connect(createGroupAction, &QAction::triggered, [this]() { createGroupFromSelection(); });
    }

    QAction *copyAction = menu->addAction("Copy");
    copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));

    QAction *cutAction = menu->addAction("Cut");
    cutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));

    connect(copyAction, &QAction::triggered, this, &BasicGraphicsScene::onCopySelectedObjects);

    connect(cutAction, &QAction::triggered, [this] {
        onCopySelectedObjects();
        onDeleteSelectedObjects();
    });

    menu->setAttribute(Qt::WA_DeleteOnClose);
    return menu;
}

QMenu *BasicGraphicsScene::createGroupMenu(QPointF const scenePos, GroupGraphicsObject *groupGo)
{
    Q_UNUSED(scenePos);
    QMenu *menu = new QMenu();

    QAction *saveGroup = nullptr;
    if (_groupingEnabled) {
        saveGroup = menu->addAction("Save group...");
    }

    // CICADA group polish: let the user pick a custom fill color via
    // QColorDialog. The chosen color persists in the group's save/load
    // JSON via NodeGroup serialization.
    QAction *colorAction = menu->addAction("Change color...");
    connect(colorAction, &QAction::triggered, [groupGo] {
        // CICADA: seed the dialog with the GROUP's actual fill color
        // via fillColor() (the same color paint() reads). The legacy
        // brush().color() returned the QGraphicsRectItem default
        // brush — which is Qt::NoBrush/black, since GroupGraphicsObject
        // never calls setBrush. Using that as the initial value made
        // the dialog open showing BLACK; if the user clicked OK
        // without moving the wheel, the group became black.
        QColor const initial = groupGo->fillColor();
        QColor const picked = QColorDialog::getColor(
            initial, nullptr, QStringLiteral("Group fill color"),
            QColorDialog::ShowAlphaChannel);
        if (picked.isValid()) {
            groupGo->setFillColor(picked);
            // Mark the color as user-chosen so setHovered() does NOT
            // overwrite it the next time the cursor enters / leaves
            // the group rect.
            groupGo->_userFillColorOverridden = true;
            groupGo->update();
        }
    });

    menu->addSeparator();

    // CICADA: count selected nodes that belong to THIS group. If any
    // are selected, expose "Remove selected from group" so the user
    // can pop nodes out without nuking the whole group. Always expose
    // "Delete group" so the user can dissolve the group entirely (the
    // contained nodes survive — see undoStack DeleteCommand path).
    int selectedMembersCount = 0;
    {
        auto const &members = groupGo->group().childNodes();
        for (NodeGraphicsObject *child : members) {
            if (child && child->isSelected()) {
                ++selectedMembersCount;
            }
        }
    }

    QAction *removeSelectedAction = nullptr;
    if (selectedMembersCount > 0) {
        removeSelectedAction = menu->addAction(
            QStringLiteral("Remove %1 selected node%2 from group")
                .arg(selectedMembersCount)
                .arg(selectedMembersCount == 1 ? QString()
                                               : QStringLiteral("s")));
        connect(removeSelectedAction, &QAction::triggered, [this, groupGo] {
            // Snapshot the IDs first because removeNodeFromGroup may
            // erase the group entirely (last node out) and invalidate
            // groupGo. Iterate via stored IDs, not live members.
            std::vector<NodeId> toRemove;
            for (NodeGraphicsObject *child : groupGo->group().childNodes()) {
                if (child && child->isSelected()) {
                    toRemove.push_back(child->nodeId());
                }
            }
            for (NodeId id : toRemove) {
                removeNodeFromGroup(id);
            }
        });
    }

    QAction *deleteGroupAction =
        menu->addAction(QStringLiteral("Delete group"));
    connect(deleteGroupAction, &QAction::triggered, [this, groupGo] {
        // Dissolve the group while leaving every contained node alive.
        // removeNodeFromGroup auto-erases the group from the scene's
        // _groups map once the last child is removed (see
        // BasicGraphicsScene::removeNodeFromGroup lines 574-576), so
        // looping the full membership cleans up the group itself
        // without needing a separate "delete the group object" call.
        std::vector<NodeId> allMembers;
        for (NodeGraphicsObject *child : groupGo->group().childNodes()) {
            if (child) {
                allMembers.push_back(child->nodeId());
            }
        }
        for (NodeId id : allMembers) {
            removeNodeFromGroup(id);
        }
    });

    menu->addSeparator();

    QAction *copyAction = menu->addAction("Copy");
    copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));

    QAction *cutAction = menu->addAction("Cut");
    cutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));

    if (saveGroup) {
        connect(saveGroup, &QAction::triggered, [this, groupGo] {
            saveGroupFile(groupGo->group().id());
        });
    }

    connect(copyAction, &QAction::triggered, this, &BasicGraphicsScene::onCopySelectedObjects);

    connect(cutAction, &QAction::triggered, [this] {
        onCopySelectedObjects();
        onDeleteSelectedObjects();
    });

    menu->setAttribute(Qt::WA_DeleteOnClose);
    return menu;
}

void BasicGraphicsScene::saveGroupFile(GroupId groupID)
{
    if (!_groupingEnabled)
        return;

    QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                    tr("Save Node Group"),
                                                    QDir::homePath(),
                                                    tr("Node Group files (*.group)"));

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith("group", Qt::CaseInsensitive))
            fileName += ".group";

        if (auto groupIt = _groups.find(groupID); groupIt != _groups.end()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(groupIt->second->saveToFile());
            } else {
                qDebug() << "Error saving group file!";
            }
        } else {
            qDebug() << "Error! Couldn't find group while saving.";
        }
    }
}

std::weak_ptr<NodeGroup> BasicGraphicsScene::loadGroupFile()
{
    if (!_groupingEnabled)
        return std::weak_ptr<NodeGroup>();

    QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                    tr("Open Node Group"),
                                                    QDir::currentPath(),
                                                    tr("Node Group files (*.group)"));

    if (!QFileInfo::exists(fileName))
        return std::weak_ptr<NodeGroup>();

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Error loading group file!";
    }

    QDir d = QFileInfo(fileName).absoluteDir();
    QString absolute = d.absolutePath();
    QDir::setCurrent(absolute);

    QByteArray wholeFile = file.readAll();

    const QJsonObject fileJson = QJsonDocument::fromJson(wholeFile).object();

    return restoreGroup(fileJson).first;
}

GroupId BasicGraphicsScene::nextGroupId()
{
    if (_nextGroupId == InvalidGroupId) {
        throw std::runtime_error("No available group identifiers");
    }

    while (_groups.count(_nextGroupId) != 0) {
        ++_nextGroupId;
        if (_nextGroupId == InvalidGroupId) {
            throw std::runtime_error("No available group identifiers");
        }
    }

    GroupId const newId = _nextGroupId;
    ++_nextGroupId;
    return newId;
}

} // namespace QtNodes
