#include "NodeGroup.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeGraphicsObject.hpp"
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <utility>

using QtNodes::DataFlowGraphModel;
using QtNodes::GroupGraphicsObject;
using QtNodes::NodeConnectionInteraction;
using QtNodes::NodeGraphicsObject;
using QtNodes::NodeGroup;
using QtNodes::NodeId;

int NodeGroup::_groupCount = 0;

NodeGroup::NodeGroup(std::vector<NodeId> nodes,
                     GroupId groupId,
                     QString name,
                     QObject *parent)
    : QObject(parent)
    , _name(std::move(name))
    , _id(groupId)
    , _childNodes(std::move(nodes))
    , _groupGraphicsObject(nullptr)
{
    _groupCount++;
}

QByteArray NodeGroup::saveToFile() const
{
    QJsonObject groupJson;

    groupJson["name"] = _name;
    groupJson["id"] = static_cast<qint64>(_id);

    // CICADA (ADR-1638): the nodes are saved from the model, which every node
    // is in, whether or not the scene made an item for it.
    QJsonArray nodesJson;
    if (auto *scene = qobject_cast<BasicGraphicsScene *>(parent())) {
        for (NodeId const nodeId : _childNodes) {
            QJsonObject nodeJson = scene->graphModel().saveNode(nodeId);
            if (nodeJson.isEmpty()) {
                nodeJson["id"] = QString::number(nodeId);
                QJsonObject obj;
                QPointF const pos
                    = scene->graphModel().nodeData(nodeId, NodeRole::Position).value<QPointF>();
                obj["x"] = pos.x();
                obj["y"] = pos.y();
                nodeJson["position"] = obj;
            }
            nodesJson.append(nodeJson);
        }
    }
    groupJson["nodes"] = nodesJson;

    QJsonArray connectionsJson;
    if (auto *scene = qobject_cast<BasicGraphicsScene *>(parent())) {
        auto groupConnections = scene->connectionsWithinGroup(_id);
        for (auto const &connection : groupConnections) {
            connectionsJson.append(toJson(*connection));
        }
    }
    groupJson["connections"] = connectionsJson;

    QJsonDocument groupDocument(groupJson);

    return groupDocument.toJson();
}

QtNodes::GroupId NodeGroup::id() const
{
    return _id;
}

GroupGraphicsObject &NodeGroup::groupGraphicsObject()
{
    return *_groupGraphicsObject;
}

GroupGraphicsObject const &NodeGroup::groupGraphicsObject() const
{
    return *_groupGraphicsObject;
}

std::vector<NodeGraphicsObject *> NodeGroup::childNodes() const
{
    std::vector<NodeGraphicsObject *> items;
    items.reserve(_childNodes.size());
    if (auto *scene = qobject_cast<BasicGraphicsScene *>(parent())) {
        for (NodeId const nodeId : _childNodes) {
            if (NodeGraphicsObject *item = scene->nodeGraphicsObject(nodeId))
                items.push_back(item);
        }
    }
    return items;
}

std::vector<NodeId> NodeGroup::nodeIDs() const
{
    return _childNodes;
}

bool NodeGroup::contains(NodeId nodeId) const
{
    return std::find(_childNodes.begin(), _childNodes.end(), nodeId) != _childNodes.end();
}

QString const &NodeGroup::name() const
{
    return _name;
}

void NodeGroup::setGraphicsObject(std::unique_ptr<GroupGraphicsObject> &&graphics_object)
{
    _groupGraphicsObject = std::move(graphics_object);
    // CICADA: do NOT auto-lock the brand-new group. The legacy
    // setGraphicsObject called lock(true) here, which propagates
    // node->lock(true) to every child via
    // GroupGraphicsObject::lock(). That made every contained node
    // non-movable from the moment the group was created — the user
    // reported "I can't drag nodes inside the group; only the whole
    // group drags". A fresh group is unlocked; the user opts in
    // explicitly by double-clicking the group rectangle to lock it.
}

bool NodeGroup::empty() const
{
    return _childNodes.empty();
}

int NodeGroup::groupCount()
{
    return _groupCount;
}

void NodeGroup::addNode(NodeGraphicsObject *node)
{
    if (node != nullptr)
        addNodeId(node->nodeId());
}

void NodeGroup::addNodeId(NodeId nodeId)
{
    _childNodes.push_back(nodeId);
    if (_groupGraphicsObject && _groupGraphicsObject->locked()) {
        if (auto *scene = qobject_cast<BasicGraphicsScene *>(parent())) {
            if (NodeGraphicsObject *item = scene->nodeGraphicsObject(nodeId))
                item->lock(true);
        }
    }
}

void NodeGroup::removeNode(NodeGraphicsObject *node)
{
    if (node != nullptr)
        removeNodeId(node->nodeId());
}

void NodeGroup::removeNodeId(NodeId nodeId)
{
    auto nodeIt = std::find(_childNodes.begin(), _childNodes.end(), nodeId);

    if (nodeIt != _childNodes.end()) {
        if (auto *scene = qobject_cast<BasicGraphicsScene *>(parent())) {
            if (NodeGraphicsObject *item = scene->nodeGraphicsObject(nodeId))
                item->unsetNodeGroup();
        }
        _childNodes.erase(nodeIt);
        if (_groupGraphicsObject)
            _groupGraphicsObject->positionLockedIcon();
    }
}
