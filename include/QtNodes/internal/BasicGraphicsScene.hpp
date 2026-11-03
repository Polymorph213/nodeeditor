#pragma once

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "CanvasConfig.hpp"
#include "ConnectionIdHash.hpp"
#include "Definitions.hpp"
#include "Export.hpp"
#include "GroupGraphicsObject.hpp"
#include "NodeGroup.hpp"
#include "QUuidStdHash.hpp"
#include "UndoCommands.hpp"
#include <QtCore/QJsonObject>
#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QMenu>

#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

class QUndoStack;

namespace QtNodes {

class AbstractConnectionPainter;
class AbstractGraphModel;
class AbstractNodePainter;
class ConnectionGraphicsObject;
class NodeGraphicsObject;
class NodeStyle;
class NodeFaceEditorHost;
class DeleteCommand;
class CopyCommand;
class NodeGroup;
class GroupGraphicsObject;
struct ConnectionId;

/// An instance of QGraphicsScene , holds connections and nodes.
class NODE_EDITOR_PUBLIC BasicGraphicsScene : public QGraphicsScene
{
    Q_OBJECT
public:
    BasicGraphicsScene(AbstractGraphModel &graphModel, QObject *parent = nullptr);

    // Scenes without models are not supported
    BasicGraphicsScene() = delete;

    ~BasicGraphicsScene();

public:
    /// @returns associated AbstractGraphModel.
    AbstractGraphModel const &graphModel() const;

    AbstractGraphModel &graphModel();

    AbstractNodeGeometry const &nodeGeometry() const;

    AbstractNodeGeometry &nodeGeometry();

    AbstractNodePainter &nodePainter();

    AbstractConnectionPainter &connectionPainter();

    void setNodePainter(std::unique_ptr<AbstractNodePainter> newPainter);

    void setConnectionPainter(std::unique_ptr<AbstractConnectionPainter> newPainter);

    void setNodeGeometry(std::unique_ptr<AbstractNodeGeometry> newGeom);

    QUndoStack &undoStack();

    /**
     * @brief Setter for the _groupingEnabled flag.
     * @param boolean to set or not the flag.
     */
    void setGroupingEnabled(bool enabled);

    /**
     * @brief Getter for the _groupingEnabled flag.
     */
    bool groupingEnabled() const { return _groupingEnabled; }

public:
    /**
     * @brief Creates a "draft" instance of ConnectionGraphicsObject.
     *
     * The scene caches a "draft" connection which has one loose end.
     * After attachment the "draft" instance is deleted and instead a
     * normal "full" connection is created.
     * Function @returns the "draft" instance for further geometry
     * manipulations.
     */
    std::unique_ptr<ConnectionGraphicsObject> const &makeDraftConnection(
        ConnectionId const newConnectionId);

    /**
     * @brief Deletes "draft" connection.
     *
     * The function is called when user releases the mouse button during
     * the construction of the new connection without attaching it to any
     * node.
     */
    void resetDraftConnection();

    /// Deletes all the nodes. Connections are removed automatically.
    void clearScene();

    /// CICADA (ADR-1638): which nodes and wires have an item, as the
    /// application's CanvasConfig said when the scene was built.
    ItemPolicy itemPolicy() const { return _itemPolicy; }

    /// CICADA (ADR-1638): the item of a node, made now when the policy made
    /// none. A caller that is about to touch a node's item (a press on a port,
    /// a wire drag) asks for it here instead of assuming it exists.
    /// \param nodeId A node of the model.
    /// \return The item; nullptr when the node is not in the model.
    NodeGraphicsObject *ensureNodeItem(NodeId nodeId);

    /// CICADA (ADR-1638): the item of a wire, made now when the policy made
    /// none.
    /// \param connectionId A wire of the model.
    /// \return The item; nullptr when the wire is not in the model.
    ConnectionGraphicsObject *ensureConnectionItem(ConnectionId connectionId);

    /// CICADA (ADR-1632): who edits the rows of a painted node. The scene does
    /// not own the host.
    /// \param host The host; null for none.
    void setFaceEditorHost(NodeFaceEditorHost *host) { _faceHost = host; }

    /// CICADA (ADR-1632): the editor host, or null.
    NodeFaceEditorHost *faceEditorHost() const { return _faceHost; }

    /// CICADA (ADR-1638): the selected nodes, by id, in id order. Copy, Cut,
    /// Delete, Duplicate and CreateGroup read this, never the items, so a
    /// scene with few or no items acts on every node a person selected.
    std::vector<NodeId> selectedNodeIds() const;

    /// CICADA (ADR-1638): the selected wires, by id.
    std::vector<ConnectionId> selectedConnectionIds() const;

    /// CICADA (ADR-1638): the selected groups, by id, in id order.
    std::vector<GroupId> selectedGroupIds() const;

    /// CICADA (ADR-1638): whether a node is selected.
    bool isNodeSelected(NodeId nodeId) const;

    /// CICADA (ADR-1638): selects or deselects a node; its item, if it has
    /// one, follows.
    void selectNode(NodeId nodeId, bool selected = true);

    /// CICADA (ADR-1638): selects or deselects a wire; its item, if it has
    /// one, follows.
    void selectConnection(ConnectionId connectionId, bool selected = true);

    /// CICADA (ADR-1638): selects every node of the model, with or without an
    /// item.
    void selectAllNodes();

    /// CICADA (ADR-1638): clears the selection of nodes, wires and groups;
    /// the items follow.
    void clearAllSelection();

    /// An item reports a change of its selection state (the selection set
    /// follows the items; the items follow the set).
    void noteNodeSelection(NodeId nodeId, bool selected);
    void noteConnectionSelection(ConnectionId connectionId, bool selected);
    void noteGroupSelection(GroupId groupId, bool selected);

    /// CICADA (ADR-1638): the group a node belongs to.
    /// \return The group's id; InvalidGroupId when the node is in none.
    GroupId groupOfNode(NodeId nodeId) const;

    /// CICADA (ADR-1638): creates a group from node ids; the nodes need no
    /// item. A scene that makes no items makes no group item either.
    std::weak_ptr<NodeGroup> createGroupFromIds(std::vector<NodeId> const &nodes,
                                                QString name = QStringLiteral(""),
                                                GroupId groupId = InvalidGroupId);

    /// CICADA (ADR-1500): whether a view that shows this scene is visible.
    bool hasVisibleView() const;

    /// CICADA (ADR-1500): gives every node whose widget waits for a view its
    /// proxy and native window. A view calls it when it is shown; a caller
    /// that renders the scene without a view calls it first.
    void embedPendingWidgets();

    /**
     * @brief Creates a list of the connections that are incident only to nodes within a
     * given group.
     * @param groupID ID of the desired group.
     * @return List of (pointers of) connections whose both endpoints belong to members of
     * the specified group.
     */
    std::vector<std::shared_ptr<ConnectionId>> connectionsWithinGroup(GroupId groupID);
    /**
     * @brief Creates a group in the scene containing the given nodes.
     * @param nodes Reference to the list of nodes to be included in the group.
     * @param name Group's name.
     * @param groupId Group's id.
     * @return Pointer to the newly-created group.
     */
    std::weak_ptr<NodeGroup> createGroup(std::vector<NodeGraphicsObject *> &nodes,
                                         QString name = QStringLiteral(""),
                                         GroupId groupId = InvalidGroupId);

    /**
     * @brief Creates a group in the scene containing the currently selected nodes.
     * @param name Group's name
     * @return Pointer to the newly-created group.
     */
    std::weak_ptr<NodeGroup> createGroupFromSelection(QString groupName = QStringLiteral(""));

    /**
     * @brief Restores a group from a JSON object.
     * @param groupJson JSON object containing the group data.
     * @return Pair consisting of a pointer to the newly-created group and the mapping
     * between old and new nodes.
     */
    std::pair<std::weak_ptr<NodeGroup>, std::unordered_map<GroupId, GroupId>> restoreGroup(
        QJsonObject const &groupJson);

    /**
     * @brief Returns a const reference to the mapping of existing groups.
     */
    std::unordered_map<GroupId, std::shared_ptr<NodeGroup>> const &groups() const;

    /**
     * @brief Loads a group from a file specified by the user.
     * @return Pointer to the newly-created group.
     */
    std::weak_ptr<NodeGroup> loadGroupFile();

    /**
     * @brief Saves a group in a .group file.
     * @param groupID Group's id.
     */
    void saveGroupFile(GroupId groupID);

    /**
     * @brief Calculates the selected nodes.
     * @return Vector containing the NodeGraphicsObject pointers related to the selected nodes.
     */
    std::vector<NodeGraphicsObject *> selectedNodes() const;

    /**
     * @brief Calculates the selected groups.
     * @return Vector containing the GroupGraphicsObject pointers related to the selected groups.
     */
    std::vector<GroupGraphicsObject *> selectedGroups() const;

    /**
     * @brief Adds a node to a group, if both node and group exists.
     * @param nodeId Node's id.
     * @param groupId Group's id.
     */
    void addNodeToGroup(NodeId nodeId, GroupId groupId);

    /**
     * @brief Removes a node from a group, if the node exists and is within a group.
     * @param nodeId Node's id.
     */
    void removeNodeFromGroup(NodeId nodeId);

public:
    /**
     * @returns NodeGraphicsObject associated with the given nodeId.
     * @returns nullptr when the object is not found.
     */
    NodeGraphicsObject *nodeGraphicsObject(NodeId nodeId);

    /**
     * @returns ConnectionGraphicsObject corresponding to `connectionId`.
     * @returns `nullptr` when the object is not found.
     */
    ConnectionGraphicsObject *connectionGraphicsObject(ConnectionId connectionId);

    Qt::Orientation orientation() const { return _orientation; }

    void setOrientation(Qt::Orientation const orientation);

public:
    /**
     * Can @return an instance of the scene context menu in subclass.
     * Default implementation returns `nullptr`.
     */
    virtual QMenu *createSceneMenu(QPointF const scenePos);

    /**
     * @brief Freezes and unfreezes the model and connections of the selected nodes.
     * @param isFreeze reference for freezing or unfreezing the model and connections of the selected nodes.
     */
    void freezeModelAndConnections(bool isFreeze);

    /**
     * @brief Creates the default menu when a node is selected.
     */
    QMenu *createStdMenu(QPointF const scenePos);

    /**
     * @brief Creates the menu when a group is selected.
     * @param groupGo reference to the GroupGraphicsObject related to the selected group.
     */
    QMenu *createGroupMenu(QPointF const scenePos, GroupGraphicsObject *groupGo);

Q_SIGNALS:
    void modified(BasicGraphicsScene *);
    void nodeMoved(NodeId const nodeId, QPointF const &newLocation);
    void nodeClicked(NodeId const nodeId);
    void nodeSelected(NodeId const nodeId);
    void nodeDoubleClicked(NodeId const nodeId);
    void nodeHovered(NodeId const nodeId, QPoint const screenPos);
    void nodeHoverLeft(NodeId const nodeId);
    void connectionHovered(ConnectionId const connectionId, QPoint const screenPos);
    void connectionHoverLeft(ConnectionId const connectionId);

    /// Signal allows showing custom context menu upon clicking a node.
    void nodeContextMenu(NodeId const nodeId, QPointF const pos);
    /// Signals to call Graphics View's zoomFit methods
    void zoomFitAllClicked();
    void zoomFitSelectedClicked();

private:
    /**
     * @brief Creates Node and Connection graphics objects.
     * 
     * Function is used to populate an empty scene in the constructor. We
     * perform depth-first AbstractGraphModel traversal. The connections are
     * created by checking non-empty node `Out` ports.
     */
    void traverseGraphAndPopulateGraphicsObjects();

    /// Redraws adjacent nodes for given `connectionId`
    void updateAttachedNodes(ConnectionId const connectionId, PortType const portType);

    /**
     * @brief Loads a JSON object that represents a node, with the option
     * to keep the stored node id or generate a new one.
     * @param nodeJson The JSON object representing a node.
     * @param keepOriginalId If true, the loaded node will have the same id as the one stored in
     * the file; otherwise, a new id will be generated
     * @return A reference to the NodeGraphicsObject related to the loaded node.
     */
    NodeId loadNodeToMap(QJsonObject nodeJson, bool keepOriginalId = false);

    /**
     * @brief Loads a connection between nodes from a JSON file.
     * @param connectionJson JSON object that stores the connection's endpoints.
     * @param nodeIdMap Map of nodes (i.e. all possible endpoints).
     */
    void loadConnectionToMap(QJsonObject const &connectionJson,
                             std::unordered_map<NodeId, NodeId> const &nodeIdMap);

public Q_SLOTS:
    /// Slot called when the `connectionId` is erased form the AbstractGraphModel.
    virtual void onConnectionDeleted(ConnectionId const connectionId);

    /// Slot called when the `connectionId` is created in the AbstractGraphModel.
    virtual void onConnectionCreated(ConnectionId const connectionId);

    virtual void onNodeDeleted(NodeId const nodeId);
    virtual void onNodeCreated(NodeId const nodeId);
    virtual void onNodePositionUpdated(NodeId const nodeId);
    virtual void onNodeUpdated(NodeId const nodeId);
    virtual void onNodeClicked(NodeId const nodeId);
    virtual void onModelReset();

    /**
     * @brief Slot called to trigger the copy command action.
     */
    void onCopySelectedObjects() { undoStack().push(new CopyCommand(this)); }

    /**
     * @brief Slot called to trigger the delete command action.
     */
    void onDeleteSelectedObjects() { undoStack().push(new DeleteCommand(this)); }

private:
    AbstractGraphModel &_graphModel;

    using UniqueNodeGraphicsObject = std::unique_ptr<NodeGraphicsObject>;
    using UniqueConnectionGraphicsObject = std::unique_ptr<ConnectionGraphicsObject>;
    using SharedGroup = std::shared_ptr<NodeGroup>;

    std::unordered_map<NodeId, UniqueNodeGraphicsObject> _nodeGraphicsObjects;
    std::unordered_map<ConnectionId, UniqueConnectionGraphicsObject> _connectionGraphicsObjects;
    GroupId nextGroupId();

    std::unordered_map<GroupId, SharedGroup> _groups{};
    GroupId _nextGroupId{0};
    std::unique_ptr<ConnectionGraphicsObject> _draftConnection;
    std::unique_ptr<AbstractNodeGeometry> _nodeGeometry;
    std::unique_ptr<AbstractNodePainter> _nodePainter;
    std::unique_ptr<AbstractConnectionPainter> _connectionPainter;
    bool _nodeDrag;
    QUndoStack *_undoStack;
    Qt::Orientation _orientation;
    bool _groupingEnabled;

    // CICADA (ADR-1638): the canvas settings read once, the editor host, the
    // selection by id and the group of each node by id. None of them needs an
    // item.
    ItemPolicy _itemPolicy = ItemPolicy::All;
    NodeFaceEditorHost *_faceHost = nullptr;
    std::unordered_set<NodeId> _selectedNodes;
    std::unordered_set<ConnectionId> _selectedConnections;
    std::unordered_set<GroupId> _selectedGroups;
    std::unordered_map<NodeId, GroupId> _groupOfNode;
};

} // namespace QtNodes
