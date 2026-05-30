#include "NodeGroup.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include <QJsonArray>
#include <QJsonDocument>

#include <utility>

using QtNodes::DataFlowGraphModel;
using QtNodes::GroupGraphicsObject;
using QtNodes::NodeConnectionInteraction;
using QtNodes::NodeGraphicsObject;
using QtNodes::NodeGroup;
using QtNodes::NodeId;

int NodeGroup::_groupCount = 0;

NodeGroup::NodeGroup(std::vector<NodeGraphicsObject *> nodes,
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

    QJsonArray nodesJson;
    for (auto const &node : _childNodes) {
        if (!node)
            continue;

        QJsonObject const nodeJson = node->save();
        if (nodeJson.isEmpty())
            continue;

        nodesJson.append(nodeJson);
    }
    groupJson["nodes"] = nodesJson;

    QJsonArray connectionsJson;
    auto groupConnections = _groupGraphicsObject->connections();
    for (auto const &connection : groupConnections) {
        connectionsJson.append(toJson(*connection));
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

std::vector<NodeGraphicsObject *> &NodeGroup::childNodes()
{
    return _childNodes;
}

std::vector<NodeId> NodeGroup::nodeIDs() const
{
    std::vector<NodeId> ret{};
    ret.reserve(_childNodes.size());

    for (auto const &node : _childNodes) {
        ret.push_back(node->nodeId());
    }

    return ret;
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
    _childNodes.push_back(node);
    if (_groupGraphicsObject && _groupGraphicsObject->locked()) {
        node->lock(true);
    }
}

void NodeGroup::removeNode(NodeGraphicsObject *node)
{
    auto nodeIt = std::find(_childNodes.begin(), _childNodes.end(), node);

    if (nodeIt != _childNodes.end()) {
        (*nodeIt)->unsetNodeGroup();
        _childNodes.erase(nodeIt);
        groupGraphicsObject().positionLockedIcon();
    }
}
