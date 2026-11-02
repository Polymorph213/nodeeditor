#include "NodeGraphicsObject.hpp"
#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "AbstractNodePainter.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeDelegateModel.hpp"
#include "NodeGroup.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"
#include "cicada/util/CommandTrace.h" // CICADA: command trace (ADR-1495)
#include "cicada/util/FixedCost.h"    // CICADA: fixed-cost switches (ADR-1500)
#include "cicada/util/WidgetResidency.h" // CICADA: live widgets (ADR-1620)
#include <QString>

#include <QtWidgets/QGraphicsEffect>
#include <QtWidgets/QtWidgets>

#include <cstdlib>

namespace QtNodes {

namespace {

/// QLabel subclass used as the persistent validation-message popup.
/// Auto-closes itself when the user clicks anywhere outside its own
/// geometry, by installing an application-wide event filter on qApp
/// that watches for MouseButtonPress events.
class ValidationPopupLabel : public QLabel
{
public:
    ValidationPopupLabel()
        : QLabel(nullptr)
    {
        if (qApp)
            qApp->installEventFilter(this);
    }
    ~ValidationPopupLabel() override
    {
        if (qApp)
            qApp->removeEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::NonClientAreaMouseButtonPress) {
            auto *w = qobject_cast<QWidget *>(watched);
            // Close if the press did NOT land on this label itself.
            if (!w || (w != this && !this->isAncestorOf(w))) {
                // Defer close to avoid deleting ourselves mid event dispatch.
                QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
            }
        }
        return QLabel::eventFilter(watched, event);
    }
};

/// Flag set by mousePressEvent when a right-click on a validation icon is
/// handled; consumed by the host app's context-menu filter.
static bool s_pendingValidationRightClick = false;

} // namespace

bool NodeGraphicsObject::consumePendingValidationRightClick()
{
    bool const v = s_pendingValidationRightClick;
    s_pendingValidationRightClick = false;
    return v;
}

NodeGraphicsObject::NodeGraphicsObject(BasicGraphicsScene &scene, NodeId nodeId)
    : _nodeId(nodeId)
    , _graphModel(scene.graphModel())
    , _nodeState(*this)
    , _locked(false)
    , _draggingIntoGroup(false)
    , _possibleGroup(nullptr)
    , _originalGroupSize()
    , _proxyWidget(nullptr)
{
    scene.addItem(this);

    setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);

    setLockedState();

    // CICADA fix: previously this was DeviceCoordinateCache. That caches
    // the node's painted pixmap at the CURRENT view scale; when the user
    // zooms in, Qt re-rasterises the cache at the new (magnified) scale
    // — expensive (pixmap alloc + blit) AND the cached image is the
    // OLD painted content. Result: at high zoom many nodes ghost or
    // render incompletely AND scrolling/zooming becomes laggy. NoCache
    // makes Qt re-paint the item directly each frame; with our R2
    // bounding-rect fix + the P2 bezier cache that's plenty fast and
    // the rendering is always correct.
    setCacheMode(QGraphicsItem::NoCache);

    QJsonObject nodeStyleJson = _graphModel.nodeData(_nodeId, NodeRole::Style).toJsonObject();

    NodeStyle nodeStyle(nodeStyleJson);

    // CICADA perf: the per-node QGraphicsDropShadowEffect was the
    // dominant frame-time cost at high zoom (Qt rasterises each item
    // through an offscreen Gaussian blur sized in screen pixels). The
    // shadow is decorative — turning it off entirely is a hard win on
    // every dense graph at every zoom level. Style file ShadowEnabled
    // flag is ignored on purpose.

    setOpacity(nodeStyle.Opacity);

    setAcceptHoverEvents(true);

    setZValue(0);

    embedQWidget(true);

    nodeScene()->nodeGeometry().recomputeSize(_nodeId);

    QPointF const pos = _graphModel.nodeData<QPointF>(_nodeId, NodeRole::Position);

    setPos(pos);

    connect(&_graphModel, &AbstractGraphModel::nodeFlagsUpdated, this, [this](NodeId const nodeId) {
        if (_nodeId == nodeId)
            setLockedState();
    });
}

NodeGraphicsObject::~NodeGraphicsObject()
{
    // CICADA (ADR-1500): a widget that never reached a proxy has no owner.
    if (_proxyWidget == nullptr && !_unembedded.isNull())
        _unembedded->deleteLater();
}

AbstractGraphModel &NodeGraphicsObject::graphModel() const
{
    return _graphModel;
}

BasicGraphicsScene *NodeGraphicsObject::nodeScene() const
{
    return dynamic_cast<BasicGraphicsScene *>(scene());
}

void NodeGraphicsObject::updateQWidgetEmbedPos()
{
    if (_proxyWidget) {
        AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
        _proxyWidget->setPos(geometry.widgetPosition(_nodeId));
    }
}

void NodeGraphicsObject::embedPendingWidget()
{
    if (_proxyWidget != nullptr || _unembedded.isNull())
        return;

    CICADA_TRACE_AS(Ui, "QtNodes.embedPendingWidget");
    // The widget is shown now, so its size may differ from the one the node
    // was given without a window.
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    QSize const before = geometry.size(_nodeId);
    prepareGeometryChange();
    embedQWidget(false);
    update();
    // CICADA (ADR-1620): a node whose size did not move has not moved its
    // wires, and the world plane has one for every object of the document.
    bool const moved = geometry.size(_nodeId) != before;
    if (moved || !cicada::widgetresidency::enabled())
        moveConnections();
    nodeScene()->noteWidgetEmbedded(moved);
}

bool NodeGraphicsObject::widgetPinned() const
{
    if (_proxyWidget == nullptr)
        return false;
    // A pop-up the widget opened lives in a proxy of its own, under this one.
    if (!_proxyWidget->childItems().isEmpty())
        return true;
    QGraphicsScene *const sc = scene();
    if (sc == nullptr)
        return false;
    if (sc->mouseGrabberItem() == _proxyWidget)
        return true;
    QGraphicsItem const *const focus = sc->focusItem();
    return focus != nullptr && (focus == _proxyWidget || _proxyWidget->isAncestorOf(focus));
}

bool NodeGraphicsObject::releaseWidget()
{
    if (_proxyWidget == nullptr || widgetPinned())
        return false;
    QWidget *const w = _proxyWidget->widget();
    if (w == nullptr)
        return false;

    CICADA_TRACE_AS(Ui, "QtNodes.releaseWidget");
    prepareGeometryChange();
    w->removeEventFilter(this);
    // The proxy owns its widget and deletes it with itself: setWidget(nullptr)
    // gives the widget back first (a Qt probe: the widget lives on, window and
    // all, until it is taken out of its window).
    _proxyWidget->setWidget(nullptr);
    delete _proxyWidget;
    _proxyWidget = nullptr;

    // The window and the backing store (2 GDI objects) stay with a widget that
    // is only taken out of a proxy. Made a child and a top-level widget again,
    // it gives both up and is as a widget that was never embedded.
    QWidget *const limbo = nodeScene()->widgetLimbo();
    w->setParent(limbo);
    w->setParent(nullptr);
    _unembedded = w;

    // What the proxy's resize filter would have done for a change made while
    // the widget was live, and `pendingWidgetChanged` does for one made later.
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    QSize const before = geometry.size(_nodeId);
    w->ensurePolished();
    w->adjustSize();
    geometry.recomputeSize(_nodeId);
    update();
    if (geometry.size(_nodeId) != before)
        moveConnections();
    return true;
}

QRectF NodeGraphicsObject::pendingWidgetRect() const
{
    if (_unembedded.isNull())
        return QRectF();
    return QRectF(nodeScene()->nodeGeometry().widgetPosition(_nodeId),
                  QSizeF(_unembedded->size()));
}

bool NodeGraphicsObject::forwardToWidget(QGraphicsSceneMouseEvent *event)
{
    if (_proxyWidget == nullptr || scene() == nullptr)
        return false;

    QGraphicsSceneMouseEvent copy(event->type());
    copy.setScenePos(event->scenePos());
    copy.setScreenPos(event->screenPos());
    copy.setLastScenePos(event->lastScenePos());
    copy.setLastScreenPos(event->lastScreenPos());
    for (Qt::MouseButton const button : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
        copy.setButtonDownScenePos(button, event->buttonDownScenePos(button));
        copy.setButtonDownScreenPos(button, event->buttonDownScreenPos(button));
        copy.setButtonDownPos(button, _proxyWidget->mapFromScene(event->buttonDownScenePos(button)));
    }
    copy.setButtons(event->buttons());
    copy.setButton(event->button());
    copy.setModifiers(event->modifiers());
    copy.setSource(event->source());
    copy.setFlags(event->flags());
    copy.setWidget(event->widget());
    copy.setTimestamp(event->timestamp());
    copy.setPos(_proxyWidget->mapFromScene(event->scenePos()));
    copy.setLastPos(_proxyWidget->mapFromScene(event->lastScenePos()));
    copy.setAccepted(false);
    scene()->sendEvent(_proxyWidget, &copy);
    return copy.isAccepted();
}

void NodeGraphicsObject::paintPendingWidget(QPainter *painter,
                                            QStyleOptionGraphicsItem const *option) const
{
    QWidget *const w = _unembedded.data();
    if (w == nullptr)
        return;

    // The proxy paints only what is exposed, in the widget's own coordinates.
    QPointF const origin = nodeScene()->nodeGeometry().widgetPosition(_nodeId);
    QRect const source = option->exposedRect.translated(-origin).toAlignedRect()
                         & QRect(QPoint(0, 0), w->size());
    if (source.isEmpty())
        return;

    painter->save();
    // A proxy ignores its node's opacity (ItemDoesntPropagateOpacityToChildren).
    painter->setOpacity(1.0);
    painter->translate(origin);
    w->render(painter, source.topLeft(), QRegion(source), QWidget::DrawChildren);
    painter->restore();
}

bool NodeGraphicsObject::hasPendingWidget() const
{
    return _proxyWidget == nullptr && !_unembedded.isNull();
}

void NodeGraphicsObject::pendingWidgetChanged()
{
    if (!hasPendingWidget())
        return;

    // A widget with no proxy has nothing to follow its size: the node takes
    // the size the widget has now, as the resize filter gives it once the
    // widget is embedded.
    prepareGeometryChange();
    _unembedded->ensurePolished();
    _unembedded->adjustSize();
    nodeScene()->nodeGeometry().recomputeSize(_nodeId);
    update();
    moveConnections();
}

void NodeGraphicsObject::embedQWidget(bool mayDefer)
{
    CICADA_TRACE_AS(Ui, "QtNodes.embedQWidget");
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    geometry.recomputeSize(_nodeId);

    if (auto w = _graphModel.nodeData(_nodeId, NodeRole::Widget).value<QWidget *>()) {
        // CICADA (ADR-1500): QGraphicsProxyWidget::setWidget shows the widget,
        // and Qt makes a native window for it (CreateWindowEx, 5 ms or more
        // each). A scene no view shows needs none: the node is given the size
        // its widget has, and the proxy waits for a view to show the scene.
        // CICADA (ADR-1620): with residency on, a node never starts with one:
        // the scene gives a window to the nodes a view shows at a readable
        // scale, a few hundred at most, and takes it back from the rest.
        if (mayDefer
            && (cicada::widgetresidency::enabled()
                || (cicada::fixedcost::enabled(cicada::fixedcost::Change::LazyNodeWidgets)
                    && !nodeScene()->hasVisibleView()))) {
            w->ensurePolished();
            w->adjustSize();
            geometry.recomputeSize(_nodeId);
            _unembedded = w;
            return;
        }
        _unembedded.clear();

        _proxyWidget = new QGraphicsProxyWidget(this);

        _proxyWidget->setWidget(w);

        _proxyWidget->setPreferredWidth(5);

        // CICADA fix (R1): force the embedded widget through one full
        // layout pass BEFORE recomputeSize() queries its dimensions.
        // Without adjustSize() the widget reports its size hint (often
        // smaller than the actual laid-out size) and the node's stored
        // NodeRole::Size ends up too small for what the widget paints,
        // so the painted widget gets clipped by boundingRect.
        w->ensurePolished();
        w->adjustSize();
        _proxyWidget->resize(w->size());

        geometry.recomputeSize(_nodeId);

        if (w->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
            unsigned int widgetHeight = geometry.size(_nodeId).height()
                                        - geometry.captionRect(_nodeId).height();

            // If the widget wants to use as much vertical space as possible, set
            // it to have the geom's equivalentWidgetHeight.
            _proxyWidget->setMinimumHeight(widgetHeight);
        }

        updateQWidgetEmbedPos();

        //update();

        _proxyWidget->setOpacity(1.0);
        _proxyWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);

        // CICADA fix (R1): if the widget grows or shrinks later (its
        // combo box expands, an inline error label appears, etc.) we
        // need to re-run recomputeSize + prepareGeometryChange so the
        // bounding rect tracks. Install an event filter on the widget
        // that catches QEvent::Resize.
        w->installEventFilter(this);
    }
}

bool NodeGraphicsObject::eventFilter(QObject *watched, QEvent *event)
{
    // CICADA fix (R1): when the embedded widget gets resized after
    // initial layout (combo expansion, error label appear, slider
    // dynamic content), the node's stored Size becomes stale and
    // boundingRect clips the widget. Re-run recomputeSize and tell
    // the scene to re-clip / re-paint.
    if (event && event->type() == QEvent::Resize && _proxyWidget
        && watched == _proxyWidget->widget()) {
        if (auto *sc = nodeScene()) {
            AbstractNodeGeometry &geom = sc->nodeGeometry();
            prepareGeometryChange();
            geom.recomputeSize(_nodeId);
            updateQWidgetEmbedPos();
            update();
        }
    }
    return QGraphicsObject::eventFilter(watched, event);
}

void NodeGraphicsObject::setLockedState()
{
    NodeFlags flags = _graphModel.nodeFlags(_nodeId);

    bool const locked = flags.testFlag(NodeFlag::Locked);

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, !locked);
}

QRectF NodeGraphicsObject::boundingRect() const
{
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    return geometry.boundingRect(_nodeId);
    //return NodeGeometry(_nodeId, _graphModel, nodeScene()).boundingRect();
}

void NodeGraphicsObject::setGeometryChanged()
{
    prepareGeometryChange();
}

void NodeGraphicsObject::setNodeGroup(std::shared_ptr<NodeGroup> group)
{
    _nodeGroup = group;
}

void NodeGraphicsObject::moveConnections() const
{
    auto const &connected = _graphModel.allConnectionIds(_nodeId);

    for (auto &cnId : connected) {
        auto cgo = nodeScene()->connectionGraphicsObject(cnId);

        if (cgo)
            cgo->move();
    }
}

void NodeGraphicsObject::reactToConnection(ConnectionGraphicsObject const *cgo)
{
    _nodeState.storeConnectionForReaction(cgo);

    update();
}

void NodeGraphicsObject::paint(QPainter *painter, QStyleOptionGraphicsItem const *option, QWidget *)
{
    QString tooltip;
    QVariant var = _graphModel.nodeData(_nodeId, NodeRole::ValidationState);
    if (var.canConvert<NodeValidationState>()) {
        auto state = var.value<NodeValidationState>();
        if (state._state != NodeValidationState::State::Valid) {
            tooltip = state._stateMessage;
        }
    }
    setToolTip(tooltip);

    painter->setClipRect(option->exposedRect);

    nodeScene()->nodePainter().paint(painter, *this);

    // CICADA (ADR-1620): over the node's body, where its proxy would paint.
    if (_proxyWidget == nullptr && !_unembedded.isNull())
        paintPendingWidget(painter, option);
}

QVariant NodeGraphicsObject::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemScenePositionHasChanged && scene()) {
        moveConnections();
        // Sync drag-induced position changes back to the graph model.
        // Without this, the model's `_nodeGeometryData[id].pos` only ever
        // reflects the initial layout position assigned at node creation;
        // any user drag is lost the next time `saveNode()` reads
        // `NodeRole::Position`. setNodeData is idempotent for an unchanged
        // value, so the QGraphicsItem ↔ model round-trip terminates without
        // recursion. Uses the existing ItemSendsScenePositionChanges flag
        // (set in NodeGraphicsObject's ctor) — no other flag changes needed.
        _graphModel.setNodeData(_nodeId, NodeRole::Position, pos());
    }

    return QGraphicsObject::itemChange(change, value);
}

void NodeGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (_locked) {
        nodeScene()->clearSelection();
        return;
    }

    // CICADA (ADR-1620): a press on the widget of a node a view shows without
    // a window (zoomed out, past the cap, or before the scene got to it) makes
    // the window now and hands the press to it, so the editor works wherever
    // it can be seen.
    if (hasPendingWidget() && pendingWidgetRect().contains(event->pos())) {
        embedPendingWidget();
        if (forwardToWidget(event)) {
            _forwardingToWidget = true;
            event->accept();
            return;
        }
    }

    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

    // --- Validation icon click ----------------------------------------------
    // The icon is drawn by DefaultNodePainter::drawValidationIcon at the
    // top-right of the node, straddling the corner:
    //   topLeft = (size.width(), -iconSize.height())  (local coords)
    //   size    = iconSize x iconSize (16x16)
    // A click inside that rect pops up a small persistent QLabel showing the
    // full validation message. The popup uses Qt::Popup so it auto-dismisses
    // when the user clicks anywhere outside it — a QToolTip cannot be used
    // because it is killed by the mouse release that immediately follows.
    {
        QVariant var = _graphModel.nodeData(_nodeId, NodeRole::ValidationState);
        if (var.canConvert<NodeValidationState>()) {
            auto state = var.value<NodeValidationState>();
            if (state._state != NodeValidationState::State::Valid
                && !state._stateMessage.isEmpty()) {
                QSize const nodeSize = geometry.size(_nodeId);
                constexpr int kIconSize = 16;
                QRectF const iconRect(nodeSize.width(), -kIconSize, kIconSize, kIconSize);
                if (iconRect.contains(event->pos())) {
                    // Right-click: copy the raw validation message to the
                    // clipboard and flash a brief confirmation popup.
                    if (event->button() == Qt::RightButton) {
                        // Tell the host app's context-menu filter to skip
                        // its menu for this right-click.
                        s_pendingValidationRightClick = true;
                        QGuiApplication::clipboard()->setText(state._stateMessage);

                        auto *toast = new ValidationPopupLabel();
                        toast->setAttribute(Qt::WA_DeleteOnClose);
                        toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint
                                              | Qt::WindowStaysOnTopHint);
                        toast->setMargin(6);
                        toast->setStyleSheet(QStringLiteral("QLabel {"
                                                            "  background: #2b2b2b;"
                                                            "  color: #e8e8e8;"
                                                            "  border: 1px solid #3498db;"
                                                            "  padding: 4px 8px;"
                                                            "  font-size: 11px;"
                                                            "}"));
                        toast->setText(QStringLiteral("Copied to clipboard"));
                        toast->adjustSize();
                        QPoint toastPos;
                        if (auto *view = (scene() && !scene()->views().isEmpty())
                                             ? scene()->views().first()
                                             : nullptr) {
                            toastPos = view->viewport()->mapToGlobal(
                                view->mapFromScene(event->scenePos()));
                        } else {
                            toastPos = QCursor::pos();
                        }
                        toast->move(toastPos + QPoint(8, 8));
                        toast->show();
                        // Auto-dismiss after 1.2s even if no outside click.
                        QTimer::singleShot(1200, toast, &QWidget::close);

                        event->accept();
                        return;
                    }

                    QString const label = (state._state == NodeValidationState::State::Error)
                                              ? QStringLiteral("<b>Error:</b> ")
                                              : QStringLiteral("<b>Warning:</b> ");

                    auto *popup = new ValidationPopupLabel();
                    popup->setAttribute(Qt::WA_DeleteOnClose);
                    popup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint
                                          | Qt::WindowStaysOnTopHint);
                    popup->setTextFormat(Qt::RichText);
                    popup->setWordWrap(true);
                    popup->setMargin(8);
                    popup->setMaximumWidth(360);
                    popup->setTextInteractionFlags(Qt::TextSelectableByMouse);
                    QString const borderCol = (state._state == NodeValidationState::State::Error)
                                                  ? QStringLiteral("#c0392b")
                                                  : QStringLiteral("#e67e22");
                    popup->setStyleSheet(QStringLiteral("QLabel {"
                                                        "  background: #2b2b2b;"
                                                        "  color: #e8e8e8;"
                                                        "  border: 1px solid %1;"
                                                        "  padding: 6px 8px;"
                                                        "  font-size: 11px;"
                                                        "}")
                                             .arg(borderCol));
                    popup->setText(label + state._stateMessage.toHtmlEscaped());
                    popup->adjustSize();

                    QPoint globalPos;
                    if (auto *view = (scene() && !scene()->views().isEmpty())
                                         ? scene()->views().first()
                                         : nullptr) {
                        globalPos = view->viewport()->mapToGlobal(
                            view->mapFromScene(event->scenePos()));
                    } else {
                        globalPos = QCursor::pos();
                    }
                    // Offset slightly so cursor isn't on top of the popup.
                    popup->move(globalPos + QPoint(8, 8));
                    popup->show();

                    event->accept();
                    return;
                }
            }
        }
    }

    for (PortType portToCheck : {PortType::In, PortType::Out}) {
        QPointF nodeCoord = sceneTransform().inverted().map(event->scenePos());

        PortIndex const portIndex = geometry.checkPortHit(_nodeId, portToCheck, nodeCoord);

        if (portIndex == InvalidPortIndex)
            continue;

        auto const &connected = _graphModel.connections(_nodeId, portToCheck, portIndex);

        // Start dragging existing connection.
        if (!connected.empty() && portToCheck == PortType::In) {
            auto const &cnId = *connected.begin();

            // Need ConnectionGraphicsObject

            NodeConnectionInteraction interaction(*this,
                                                  *nodeScene()->connectionGraphicsObject(cnId),
                                                  *nodeScene());

            if (_graphModel.detachPossible(cnId))
                interaction.disconnect(portToCheck);
        } else // initialize new Connection
        {
            if (portToCheck == PortType::Out) {
                auto const outPolicy = _graphModel
                                           .portData(_nodeId,
                                                     portToCheck,
                                                     portIndex,
                                                     PortRole::ConnectionPolicyRole)
                                           .value<ConnectionPolicy>();

                if (!connected.empty() && outPolicy == ConnectionPolicy::One) {
                    for (auto &cnId : connected) {
                        _graphModel.deleteConnection(cnId);
                    }
                }
            } // if port == out

            ConnectionId const incompleteConnectionId = makeIncompleteConnectionId(_nodeId,
                                                                                   portToCheck,
                                                                                   portIndex);

            // From the moment of creation a draft connection
            // grabs the mouse events and waits for the mouse button release
            nodeScene()->makeDraftConnection(incompleteConnectionId);
        }
    }

    if (_graphModel.nodeFlags(_nodeId) & NodeFlag::Resizable) {
        auto pos = event->pos();
        bool const hit = geometry.resizeHandleRect(_nodeId).contains(QPoint(pos.x(), pos.y()));
        _nodeState.setResizing(hit);
    }

    QGraphicsObject::mousePressEvent(event);

    if (isSelected()) {
        Q_EMIT nodeScene()->nodeSelected(_nodeId);
    }
}

void NodeGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (_forwardingToWidget) {
        (void) forwardToWidget(event);
        event->accept();
        return;
    }

    // Deselect all other items after this one is selected.
    // Unless we press a CTRL button to add the item to the selected group before
    // starting moving.
    if (!isSelected()) {
        if (!event->modifiers().testFlag(Qt::ControlModifier))
            scene()->clearSelection();

        setSelected(true);
    }

    if (_nodeState.resizing()) {
        auto diff = event->pos() - event->lastPos();

        if (auto w = _graphModel.nodeData<QWidget *>(_nodeId, NodeRole::Widget)) {
            prepareGeometryChange();

            auto oldSize = w->size();

            oldSize += QSize(diff.x(), diff.y());

            w->resize(oldSize);

            AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

            // Passes the new size to the model.
            geometry.recomputeSize(_nodeId);

            update();

            moveConnections();

            event->accept();
        }
    } else {
        QGraphicsObject::mouseMoveEvent(event);

        if (event->lastPos() != event->pos()) {
            auto diff = event->pos() - event->lastPos();
            if (nodeScene()->groupingEnabled()) {
                if (auto nodeGroup = _nodeGroup.lock(); nodeGroup) {
                    auto &ggo = nodeGroup->groupGraphicsObject();
                    ggo.moveConnections();
                    if (ggo.locked()) {
                        ggo.moveNodes(diff);
                    } else {
                        // CICADA: when an unlocked-group child node
                        // moves, tell the group its cached bounding
                        // rect is stale and force a repaint. Without
                        // these calls the group rectangle stays at
                        // the OLD size and visually does not expand /
                        // retract to follow the node.
                        ggo.invalidateGeometry();
                        ggo.update();
                    }
                } else {
                    moveConnections();
                    // if it intersects with a group, expand group
                    QList<QGraphicsItem *> overlapItems = collidingItems();
                    for (auto &item : overlapItems) {
                        auto ggo = qgraphicsitem_cast<GroupGraphicsObject *>(item);
                        if (ggo != nullptr) {
                            if (!ggo->locked()) {
                                if (!_draggingIntoGroup) {
                                    _draggingIntoGroup = true;
                                    _possibleGroup = ggo;
                                    _originalGroupSize = _possibleGroup->mapRectToScene(ggo->rect());
                                    _possibleGroup->setPossibleChild(this);
                                    break;
                                } else {
                                    if (ggo == _possibleGroup) {
                                        if (!boundingRect().intersects(
                                                mapRectFromScene(_originalGroupSize))) {
                                            _draggingIntoGroup = false;
                                            _originalGroupSize = QRectF();
                                            _possibleGroup->unsetPossibleChild();
                                            _possibleGroup = nullptr;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                moveConnections();
            }
        }
        event->ignore();
    }

    QRectF r = nodeScene()->sceneRect();

    r = r.united(mapToScene(boundingRect()).boundingRect());

    nodeScene()->setSceneRect(r);
}

void NodeGraphicsObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (_forwardingToWidget) {
        _forwardingToWidget = false;
        (void) forwardToWidget(event);
        event->accept();
        return;
    }

    _nodeState.setResizing(false);

    QGraphicsObject::mouseReleaseEvent(event);

    // position connections precisely after fast node move
    moveConnections();

    if (nodeScene()->groupingEnabled() && _draggingIntoGroup && _possibleGroup
        && _nodeGroup.expired()) {
        nodeScene()->addNodeToGroup(_nodeId, _possibleGroup->group().id());
        _possibleGroup->unsetPossibleChild();
        _draggingIntoGroup = false;
        _originalGroupSize = QRectF();
        _possibleGroup = nullptr;
    }

    nodeScene()->nodeClicked(_nodeId);
}

void NodeGraphicsObject::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    // bring all the colliding nodes to background
    QList<QGraphicsItem *> overlapItems = collidingItems();

    for (QGraphicsItem *item : overlapItems) {
        if (auto group = qgraphicsitem_cast<GroupGraphicsObject *>(item)) {
            Q_UNUSED(group);
            continue;
        }

        if (item->zValue() > 0.0) {
            item->setZValue(0.0);
        }
    }

    // bring this node forward
    setZValue(1.0);

    _nodeState.setHovered(true);

    update();

    Q_EMIT nodeScene()->nodeHovered(_nodeId, event->screenPos());

    event->accept();
}

void NodeGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    _nodeState.setHovered(false);

    setZValue(0.0);

    update();

    Q_EMIT nodeScene()->nodeHoverLeft(_nodeId);

    event->accept();
}

void NodeGraphicsObject::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto pos = event->pos();

    //NodeGeometry geometry(_nodeId, _graphModel, nodeScene());
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

    if ((_graphModel.nodeFlags(_nodeId) | NodeFlag::Resizable)
        && geometry.resizeHandleRect(_nodeId).contains(QPoint(pos.x(), pos.y()))) {
        setCursor(QCursor(Qt::SizeFDiagCursor));
    } else {
        setCursor(QCursor());
    }

    event->accept();
}

void NodeGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseDoubleClickEvent(event);

    Q_EMIT nodeScene()->nodeDoubleClicked(_nodeId);
}

void NodeGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    Q_EMIT nodeScene()->nodeContextMenu(_nodeId, mapToScene(event->pos()));
}

void NodeGraphicsObject::lock(bool locked)
{
    _locked = locked;

    setFlag(QGraphicsItem::ItemIsFocusable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
}

QJsonObject NodeGraphicsObject::save() const
{
    QJsonObject nodeJson = _graphModel.saveNode(_nodeId);
    if (nodeJson.isEmpty()) {
        nodeJson["id"] = QString::number(_nodeId);
        QJsonObject obj;
        obj["x"] = pos().x();
        obj["y"] = pos().y();
        nodeJson["position"] = obj;
    }

    return nodeJson;
}
} // namespace QtNodes
