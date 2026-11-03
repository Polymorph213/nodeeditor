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
#include "FaceLayout.hpp"
#include "NodeState.hpp"

#include <QColor>

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

    /// CICADA (ADR-1631): whether this node paints its body from rows and owns
    /// no widget (the canvas is `painted`).
    bool painted() const { return _painted; }

    /// CICADA (ADR-1631): the flat state colour behind the rows (normal,
    /// hidden, error ...), set by the application as it did the style sheet of
    /// a legacy node's widget.
    /// \param color The colour.
    void setFaceBackground(QColor const &color);

    /// CICADA (ADR-1631): the colour set by setFaceBackground().
    QColor faceBackground() const { return _faceBackground; }

    /// CICADA (ADR-1631): where the rows are, in item coordinates.
    /// \return The body rectangle; empty when the node has no rows.
    QRectF faceRect() const;

    /// CICADA (ADR-1631): what is under a point.
    /// \param pos In item coordinates.
    /// \return The row and the part; row -1 outside the rows.
    FaceHit faceHitAt(QPointF const &pos) const;

    /// CICADA (ADR-1631): the node's rows, their visibility or their values
    /// changed: the size is measured again, the wires follow and the node is
    /// repainted. Does nothing for a node that is not painted.
    void faceChanged();

    /// CICADA (ADR-1631): an editor is open on a row, so the row does not
    /// paint its text.
    /// \param row The row; -1 for none.
    /// \param component The Vec3 component; -1 for none.
    void setFaceEditing(int row, int component);

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
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    bool sceneEvent(QEvent *event) override;

    // CICADA fix (R1): catch QEvent::Resize on the embedded widget so
    // we re-run recomputeSize + prepareGeometryChange when the widget
    // grows / shrinks (combo expansion, error label appear, etc.).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// \param mayDefer Whether the proxy may wait until a view shows the
    ///        scene (ADR-1500). False embeds now.
    void embedQWidget(bool mayDefer);
    void setLockedState();

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

    // CICADA (ADR-1631): the painted canvas. A painted node has no widget; its
    // rows are painted over the body and a press on one goes to the scene's
    // NodeFaceEditorHost.
    bool _painted = false;
    bool _faceGrab = false;
    FaceHit _facePressed;
    FaceHit _faceHover;
    QColor _faceBackground = QColor(60, 60, 60);
    int _editRow = -1;
    int _editComponent = -1;

    std::weak_ptr<NodeGroup> _nodeGroup{};
};
} // namespace QtNodes
