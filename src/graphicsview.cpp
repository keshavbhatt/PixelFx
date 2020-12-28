#include "graphicsview.h"

#include <QTimer>

graphicsView::graphicsView(QWidget *parent) :
    QGraphicsView(parent)
{
    //pixmap update animation
    animation = new QVariantAnimation(this);
    animation->setEasingCurve(QEasingCurve::Linear);

    //scrollbar off
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    //view settings
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::ScrollHandDrag);

    //the scene
    scene = new QGraphicsScene(this);
    setScene(scene);

    //the background gradiant
    setStyleSheet("QGraphicsView{border:none;background-color: qlineargradient(spread:reflect, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(17, 18, 20, 240), stop:0.5 rgba(17, 18, 20, 96), stop:1 rgba(17, 18, 20, 240));}");

    //Ctrl+Plus and Ctrl+Minus will work, other keyboard with standard keys will work.
    m_pZoomInSc = new QShortcut( QKeySequence::ZoomIn, this, SLOT( shortCutZoomIn() ) );
    m_pZoomOutSc = new QShortcut( QKeySequence::ZoomOut, this, SLOT( shortCutZoomOut() ) );

}


//load image to graphics scene
bool graphicsView::loadImage(QImage img)
{
    scene->clear();
    if(img.isNull() == false){
        pixItem = new QGraphicsPixmapItem();
        pixItem->setFlags(QGraphicsItem::ItemIsMovable/* | QGraphicsItem::ItemIsSelectable*/ | pixItem->flags());
        pixItem->setTransformationMode(Qt::SmoothTransformation);
        updateImageView(img); //pixItem will take maximum 300ms to init due to animation stuff
        scene->addItem(pixItem);
        if(fitToViewChecked){
            fitInView(img.rect(),Qt::KeepAspectRatio);//so we set it to fit using img object
        }
        emit itemAdded(); // signal to update toolbars
        return true;
    }else{
        return false;
    }
}


//update image in scene
void graphicsView::updateImageView(QImage image)
{
    animation->setDuration(100);
    animation->setStartValue(1.0);
    animation->setEndValue(0.7);
    animation->setDirection(QVariantAnimation::Forward);
    animation->disconnect();
    connect(animation,&QVariantAnimation::valueChanged,[=](QVariant val)
    {
       int step = val.toReal() * 100.0;
       pixItem->setOpacity(val.toReal());
       if(step == 70){
           animation->stop();
           animation->setDuration(300);
           pixItem->setPixmap(QPixmap::fromImage(image));
           animation->setDirection(QVariantAnimation::Backward);
           animation->start();
       }
    });
    animation->start();

    emit pixUpdated(image);
    image.detach();
}


//Shortcut Zoom In
void graphicsView::shortCutZoomIn()
{
    if (isSceneEmpty()) return;
    //zoom
    setTransformationAnchor( QGraphicsView::AnchorUnderMouse) ;
    // Scale the view / do the zoom
    double scaleFactor = 1.5;
    // Zoom in
    scale( scaleFactor, scaleFactor );
}

//Shortcut Zoom Out
void graphicsView::shortCutZoomOut()
{
    if (isSceneEmpty()) return;
    //zoom
    setTransformationAnchor( QGraphicsView::AnchorUnderMouse );
    // Scale the view / do the zoom
    double scaleFactor = 1.5;
    // Zoom in
    scale( 1.0 / scaleFactor, 1.0 / scaleFactor );
}


//The mousewheel event
void graphicsView::wheelEvent(QWheelEvent *event)
{
    if (isSceneEmpty()){
        event->ignore();
        return;
    }
    //zoom
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    // Scale the view / do the zoom
    double scaleFactor = 1.05;
    if(event->angleDelta().y() > 0)
    {
        // Zoom in
        scale(scaleFactor, scaleFactor);
    }
    else if(event->angleDelta().y() < 0)
    {
        // Zooming out
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
}


//Reset the zoom to exact 100%
void graphicsView::resetZoom()
{
    //resetTransform(); //->kills anchor
    //calculating this does not kill anchor -> latest anchor will be used
    qreal percentZoom = 100.0;
    qreal targetScale = (qreal)percentZoom / 100.0;
    qreal scaleFactor = targetScale / transform().m11();
    scale( scaleFactor, scaleFactor );
}


/**
 * @brief Rotate all selected items in the scene
 */
void graphicsView::rotateSelectedItems()
{
     foreach(QGraphicsItem* item, scene->selectedItems()) {
       rotateItem(item);
     }
}

/**
 * @brief Return true if scene in view is empty
 */
bool graphicsView::isSceneEmpty()
{
    //return false if scene is not initialzed
    if(scene == nullptr)
        return true;

    return scene->items().isEmpty();
}

/**
 * @brief graphicsView::getCurrentImage getter method to get _currentImage
 * @return _currentImage with effects
 */
QPixmap graphicsView::getCurrentImage()
{
    return pixItem->pixmap();
}

/**
 *  @brief Rotate selected item by given angle.
 *  This function will try to rotate first item if given item is null
 */
void graphicsView::rotateItem(QGraphicsItem* item, qreal angle)
{
    if(isSceneEmpty())
        return;

    if(item == nullptr){
       item = scene->items().first();
    }

    QRectF rect = item->mapToScene(item->boundingRect()).boundingRect();
    QPointF center = rect.center();

    QTransform t;
    t.translate(center.x(), center.y());
    t.rotate(angle);
    t.translate(-center.x(), -center.y());
    item->setPos(t.map(item->pos()));
    item->setRotation(item->rotation() + angle);
}


/**
 * @brief Return the roation angle of selected item else first item
*/
int graphicsView::rotation(QGraphicsItem* item)
{
    if(isSceneEmpty())
        return 0;

    if(item == nullptr){
       item = scene->items().first();
    }

    return item->rotation();
}


/**
 * @brief Fits main PixItem(primary item) to view for now
 * This need recheck, when we will impliment multiple item in scene
 */
void graphicsView::fitToView()
{
    if(!isSceneEmpty())
    {
        //good to fitInView scene with multiple items
        //fitInView(scene->itemsBoundingRect(),Qt::KeepAspectRatio);

        //good for single Item in scene
        fitInView(pixItem,Qt::KeepAspectRatio);

//        //manual move item
//        if(fitToViewChecked)
//            //manual move item
//            pixItem->setPos(sceneRect().center()-pixItem->pixmap().rect().center());
    }
}

void graphicsView::centerItem(QRect rect)
{
    if(isSceneEmpty())
        return;
     fitInView(rect,Qt::KeepAspectRatio);
//     pixItem->setPos(sceneRect().center()-rect.center());
}

/**
 * @brief graphicsView::setFitToView
 * @param fitToView
 * Fits changes fitToView boolen when graphicsViewControls emit setFitToView
 */
void graphicsView::setFitToView(bool fitToView)
{
    fitToViewChecked = fitToView;
}

/**
 * @brief Reimplemented resize event to make PixItem to fit to view for now
 */
void graphicsView::resizeEvent(QResizeEvent *event)
{
    if( !isSceneEmpty() && fitToViewChecked )
        fitToView();
    QGraphicsView::resizeEvent(event);
}
