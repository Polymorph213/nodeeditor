#pragma once

#include "BasicGraphicsScene.hpp"
#include "Definitions.hpp"
#include "NodeGroup.hpp"
#include <QPen>
#include <QtWidgets/QGraphicsRectItem>

/**
 * @brief The IconGraphicsItem class is an auxiliary class that implements
 * custom behaviour to a fixed-size icon object.
 */
class IconGraphicsItem : public QGraphicsPixmapItem
{
public:
    IconGraphicsItem(QGraphicsItem *parent = nullptr);

    IconGraphicsItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

    /**
   * @brief Returns the factor by which the original image was scaled
   * to fit the desired icon size.
   */
    double scaleFactor() const;

    /**
   * @brief Returns the icon size.
   */
    static constexpr double iconSize() { return _iconSize; }

private:
    double _scaleFactor{};

private:
    static constexpr double _iconSize = 24.0;
};

namespace QtNodes {

class BasicGraphicsScene;
class NodeGroup;
class NodeGraphicsObject;

/**
 * @brief The GroupGraphicsObject class handles the graphical part of a node group.
 * Each node group is associated with a unique GroupGraphicsObject.
 */
class GroupGraphicsObject
    : public QObject
    , public QGraphicsRectItem
{
    Q_OBJECT

public:
    /**
   * @brief Constructor that creates a group's graphical object that should be
   * included in the given scene and associated with the given NodeGroup object.
   * @param scene Reference to the scene that will include this object.
   * @param nodeGroup Reference to the group associated with this object.
   */
    GroupGraphicsObject(BasicGraphicsScene &scene, NodeGroup &nodeGroup);

    GroupGraphicsObject(const GroupGraphicsObject &ggo) = delete;
    GroupGraphicsObject &operator=(const GroupGraphicsObject &other) = delete;
    GroupGraphicsObject(GroupGraphicsObject &&ggo) = delete;
    GroupGraphicsObject &operator=(GroupGraphicsObject &&other) = delete;

    ~GroupGraphicsObject() override;

    /**
   * @brief Returns a reference to this object's associated group.
   */
    NodeGroup &group();

    /**
   * @brief Returns a const reference to this object's associated group.
   */
    NodeGroup const &group() const;

    /**
   * @copydoc QGraphicsItem::boundingRect()
   */
    QRectF boundingRect() const override;

    enum { Type = UserType + 3 };

    /**
   * @copydoc QGraphicsItem::type()
   */
    int type() const override { return Type; }

    /**
   * @brief Sets the group's area color.
   * @param color Color to paint the group area.
   */
    void setFillColor(const QColor &color);

    /// Returns the current fill color (the one paint() uses). Distinct
    /// from QGraphicsRectItem::brush().color() because this class
    /// never calls setBrush — it paints with _currentFillColor
    /// directly. Used by the "Change color..." menu so the picker
    /// opens seeded with the actual current color rather than the
    /// default (black/transparent) brush.
    QColor fillColor() const { return _currentFillColor; }

    /// Records that the user has chosen a custom fill color via the
    /// "Change color..." menu. When true, setHovered / lock() must NOT
    /// overwrite _currentFillColor with their hardcoded hover /
    /// locked / unlocked constants — otherwise the user's color
    /// vanishes the moment the cursor enters the group.
    bool _userFillColorOverridden = false;

    /// Public wrapper around the protected QGraphicsItem
    /// prepareGeometryChange(). Called by NodeGraphicsObject when a
    /// child node moves so the group invalidates its cached bounding
    /// rect and the rectangle visually expands / retracts to follow.
    void invalidateGeometry() { prepareGeometryChange(); }

    /**
   * @brief Sets the group's border color.
   * @param color Color to paint the group's border.
   */
    void setBorderColor(const QColor &color);

    /**
   * @brief Updates the position of all the connections that are incident
   * to the nodes of this group.
   */
    void moveConnections();

    /**
   * @brief Moves the position of all the nodes of this group by the amount given.
   * @param offset 2D vector representing the amount by which the group has moved.
   */
    void moveNodes(const QPointF &offset);

    /**
   * @brief Sets the lock state of the group. Locked groups don't allow individual
   * interactions with its nodes, and can only be moved or selected as a whole.
   * @param locked Determines whether this group should be locked.
   */
    void lock(bool locked);

    /**
   * @brief Returns the lock state of the group. Locked groups don't allow individual
   * interactions with its nodes, and can only be moved or selected as a whole.
   */
    bool locked() const;

    /**
   * @brief Updates the position of the group's padlock icon to
   * the top-right corner.
   */
    void positionLockedIcon();

    /**
   * @brief Sets the group hovered state. When the mouse pointer hovers over
   * (or leaves) a group, the group's appearance changes.
   * @param hovered Determines the hovered state.
   */
    void setHovered(bool hovered);

    /**
   * @brief When a node is dragged within the borders of a group, the group's
   * area expands to include the node until the node leaves the area or is
   * released in the group. This function temporarily sets the node as the
   * possible newest member of the group, making the group's area expand.
   * @param possibleChild Pointer to the node that may be included.
   */
    void setPossibleChild(NodeGraphicsObject *possibleChild);

    /**
   * @brief Clears the possibleChild variable.
   * @note See setPossibleChild(NodeGraphicsObject*).
   */
    void unsetPossibleChild();

    /**
   * @brief Returns all the connections that are incident strictly within the
   * nodes of this group.
   */
    std::vector<std::shared_ptr<ConnectionId>> connections() const;

    /**
   * @brief Sets the position of the group.
   * @param position The desired (top-left corner) position of the group, in
   * scene coordinates.
   */
    void setPosition(const QPointF &position);

protected:
    /** @copydoc QGraphicsItem::paint() */
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = nullptr) override;

    /** @copydoc QGraphicsItem::hoverEnterEvent() */
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    /** @copydoc QGraphicsItem::hoverLeaveEvent() */
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    /// CICADA: if a node is under the cursor at higher Z, propagate
    /// the press to it instead of starting a group drag. Without this
    /// override Qt's BSP hit-test occasionally returned the group
    /// rectangle for clicks meant for an enclosed node.
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    /// CICADA: hit-test returns false when the point lies inside any
    /// member node's bounding rect. Makes the group rectangle act
    /// "transparent" wherever a node is — Qt then picks the node as
    /// the topmost item there, regardless of any BSP-cache weirdness
    /// or proxy-widget sub-item interference. The group is still
    /// clickable on its empty interior margins (where moveNodes can
    /// drag the whole group).
    bool contains(const QPointF &point) const override;

    /** @copydoc QGraphicsItem::mouseMoveEvent() */
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    /** @copydoc QGraphicsItem::mouseDoubleClickEvent() */
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

public:
    /**
   * @brief _currentFillColor Holds the current color of the group background.
   */
    QColor _currentFillColor;

    /**
   * @brief _currentBorderColor Holds the current color of the group border.
   */
    QColor _currentBorderColor;

    const QColor kUnlockedFillColor = QColor("#20a5b084");
    const QColor kUnlockedHoverColor = QColor("#2083a4af");

    const QColor kLockedFillColor = QColor("#3fe0bebc");
    const QColor kLockedHoverColor = QColor("#3feecdcb");

    const QColor kSelectedBorderColor = QColor("#eeffa500");
    const QColor kUnselectedBorderColor = QColor("#eeaaaaaa");

    /**
   * @brief _borderPen Object that dictates how the group border should be drawn.
   */
    QPen _borderPen;

private:
    /**
   * @brief _scene Reference to the scene object in which this object is included.
   */
    BasicGraphicsScene &_scene;

    /**
   * @brief _group Reference to the group instance that corresponds to this object.
   */
    NodeGroup &_group;

    IconGraphicsItem *_lockedGraphicsItem;
    IconGraphicsItem *_unlockedGraphicsItem;

    QPixmap _lockedIcon{QStringLiteral("://padlock-lock.png")};
    QPixmap _unlockedIcon{QStringLiteral("://padlock-unlock.png")};

    /**
   * @brief _possibleChild Pointer that temporarily is set to an existing node when
   * the user drags the node to this group's area.
   */
    NodeGraphicsObject *_possibleChild;

    /**
   * @brief _locked Holds the lock state of the group. Locked groups don't allow individual
   * interactions with its nodes, and can only be moved or selected as a whole.
   */
    bool _locked;

    static constexpr double _groupBorderX = 25.0;
    static constexpr double _groupBorderY = _groupBorderX * 0.8;
    static constexpr double _roundedBorderRadius = _groupBorderY;
    static constexpr QMarginsF _margins = QMarginsF(_groupBorderX,
                                                    _groupBorderY + IconGraphicsItem::iconSize(),
                                                    _groupBorderX + IconGraphicsItem::iconSize(),
                                                    _groupBorderY);

    static constexpr double _defaultWidth = 50.0;
    static constexpr double _defaultHeight = 50.0;

    // CICADA: group must sit BENEATH nodes (Z=0) and connections
    // (Z=-1) so the fill rectangle never visually covers the
    // contained items. Bumped from 2.0 to 10.0 — node Z briefly hits
    // +1.0 during drag (NodeGraphicsObject sets it on press), so the
    // group's -2.0 was actually OK on paper but visually marginal;
    // -10.0 gives a clear stacking gap and prevents any future
    // node-Z bump from accidentally putting a node beneath the
    // group rect.
    static constexpr double _groupAreaZValue = 10.0;
};

} // namespace QtNodes
