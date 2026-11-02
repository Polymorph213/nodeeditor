#pragma once

#include "NodeDelegateModel.hpp"
#include "NodeGroup.hpp"
#include "NodeState.hpp"
#include <QIcon>
#include <QPointer>
#include <QRectF>
#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsObject>
#include <QtWidgets/QWidget>

#include "Export.hpp"
#include "NodeState.hpp"

class QGraphicsProxyWidget;

namespace QtNodes {

class BasicGraphicsScene;
class AbstractGraphModel;
class NodeGroup;
class NodeDelegateModel;
class GroupGraphicsObject;

class NODE_EDITOR_PUBLIC NodeGraphicsObject : public QGraphicsObject
{
    Q_OBJECT
public:
    // Needed for qgraphicsitem_cast
    enum { Type = UserType + 1 };

    int type() const override { return Type; }

public:
    NodeGraphicsObject(BasicGraphicsScene &scene, NodeId node);

    ~NodeGraphicsObject() override;

public:
    AbstractGraphModel &graphModel() const;

    BasicGraphicsScene *nodeScene() const;

    NodeId nodeId() { return _nodeId; }

    NodeId nodeId() const { return _nodeId; }

    NodeState &nodeState() { return _nodeState; }

    NodeState const &nodeState() const { return _nodeState; }

    QRectF boundingRect() const override;

    void setGeometryChanged();

    /// Visits all attached connections and corrects
    /// their corresponding end points.
    void moveConnections() const;

    /// Repaints the node once with reacting ports.
    void reactToConnection(ConnectionGraphicsObject const *cgo);

    /// Lockes/unlockes nodes in a selected node group.
    void lock(bool locked);

    void updateQWidgetEmbedPos();

    /// CICADA (ADR-1500): gives the node's widget its proxy, and with it
    /// its native window, when the node was built while no view showed the
    /// scene. Does nothing for a node whose widget is embedded already.
    void embedPendingWidget();

    /// CICADA (ADR-1500): whether the node's widget waits for a view (it has
    /// no proxy yet).
    bool hasPendingWidget() const;

    /// CICADA (ADR-1500): the widget of a node that has no proxy changed
    /// (its style sheet, say): the node takes the size the widget now has.
    void pendingWidgetChanged();

    /// CICADA (ADR-1620): whether the node's widget has its proxy, and with it
    /// a native window, now.
    bool hasLiveWidget() const { return _proxyWidget != nullptr; }

    /// CICADA (ADR-1620): takes the proxy and the native window away from a
    /// live widget; the node keeps the widget, draws it from the widget itself
    /// and gives it a window again when a view needs one. Does nothing, and
    /// returns false, while the widget is pinned.
    bool releaseWidget();

    /// CICADA (ADR-1620): whether a person is using the live widget now (it has
    /// the scene's focus, holds the mouse, or hosts a pop-up), so that its
    /// window must stay.
    bool widgetPinned() const;

    /// Saves node in a QJsonObject save file.
    QJsonObject save() const;

    /** @brief Setter for the NodeGroup object.
     *  @param shared pointer to the node group.
     */
    void setNodeGroup(std::shared_ptr<NodeGroup> group);

    /// Unsets NodeGroup, setting it to an empty pointer.
    void unsetNodeGroup() { _nodeGroup = std::weak_ptr<NodeGroup>(); }

    /// Getter for the NodeGroup object.
    std::weak_ptr<NodeGroup> nodeGroup() const { return _nodeGroup; }

    /// CICADA: returns true if the most recent right-click landed on a
    /// validation icon (top-right of a node) and was handled by this
    /// class (copied message to clipboard). Host applications should
    /// call this from their context-menu event filter and skip showing
    /// their menu when it returns true. Calling this clears the flag.
    static bool consumePendingValidationRightClick();

protected:
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = 0) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    // CICADA fix (R1): catch QEvent::Resize on the embedded widget so
    // we re-run recomputeSize + prepareGeometryChange when the widget
    // grows / shrinks (combo expansion, error label appear, etc.).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// \param mayDefer Whether the proxy may wait until a view needs it
    ///        (ADR-1500, ADR-1620). False embeds now.
    void embedQWidget(bool mayDefer);
    void setLockedState();

    /// CICADA (ADR-1620): draws the widget of a node that has no proxy, the
    /// way its proxy would.
    void paintPendingWidget(QPainter *painter, QStyleOptionGraphicsItem const *option) const;

    /// CICADA (ADR-1620): the widget's place in the node, in node coordinates.
    QRectF pendingWidgetRect() const;

    /// CICADA (ADR-1620): hands a mouse event on the widget to its proxy, in
    /// the proxy's own coordinates. Returns whether the proxy accepted it.
    bool forwardToWidget(QGraphicsSceneMouseEvent *event);

private:
    NodeId _nodeId;

    AbstractGraphModel &_graphModel;

    NodeState _nodeState;

    bool _locked;

    bool _draggingIntoGroup;
    GroupGraphicsObject *_possibleGroup;
    QRectF _originalGroupSize;

    // either nullptr or owned by parent QGraphicsItem
    QGraphicsProxyWidget *_proxyWidget;

    // CICADA (ADR-1500): the node's widget while it has no proxy. Nothing
    // owns it then, so the destructor does.
    QPointer<QWidget> _unembedded;

    // CICADA (ADR-1620): a press on a widget without a proxy gave it one, and
    // the proxy took the press; the moves and the release that follow go to it.
    bool _forwardingToWidget = false;

    std::weak_ptr<NodeGroup> _nodeGroup{};
};
} // namespace QtNodes
