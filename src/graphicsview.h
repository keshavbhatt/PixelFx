#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include <QObject>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QEvent>
#include <QWheelEvent>
#include <QGraphicsSceneWheelEvent>
#include <QImageReader>
#include <QShortcut>
#include <QPropertyAnimation>
#include <QVariantAnimation>

#include "cropwidget.h"

class graphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    graphicsView(QWidget *parent = nullptr);
signals:
    void pixUpdated(QImage image);
    void itemAdded();
public slots:
    bool loadImage(QImage img);
    void updateImageView(QImage image);
    void rotateSelectedItems();
    void rotateItem(QGraphicsItem *item = nullptr, qreal angle = 90);
    int  rotation(QGraphicsItem *item = nullptr);
    void fitToView();
    void resetZoom();
    bool isSceneEmpty();
    void setFitToView(bool fitToView);
    void crop();
public:
    bool fitToViewChecked = true;
protected slots:
    void resizeEvent(QResizeEvent *event);
    void wheelEvent(QWheelEvent *event);
private slots:
    void shortCutZoomOut();
    void shortCutZoomIn();
private:
    QGraphicsScene* scene = nullptr;
    QImage  *_currentImage;
    QGraphicsPixmapItem* pixItem;
    QShortcut *m_pZoomInSc;
    QShortcut *m_pZoomOutSc;
    QVariantAnimation *animation;


};

#endif // GRAPHICSVIEW_H