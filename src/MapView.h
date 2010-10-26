#ifndef MERKATOR_MAPVIEW_H_
#define MERKATOR_MAPVIEW_H_

#include "Maps/Projection.h"
#include "IRenderer.h"

#include <QPixmap>
#include <QWidget>
#include <QShortcut>
#include <QLabel>

class MainWindow;
class Feature;
class Way;
class Document;
class PropertiesDock;
class InfoDock;
class MapAdapter;
class Interaction;
class ImageMapLayer;

class MapViewPrivate;

class MapView :	public QWidget
{
    Q_OBJECT

    public:
        MapView(QWidget* parent);
    public:
        ~MapView();

        MainWindow* main();
        void setDocument(Document* aDoc);
        Document* document();
        void launch(Interaction* anInteraction);
        Interaction* interaction();

        void buildFeatureSet();
        void drawCoastlines(QPainter & painter);
        void drawFeatures(QPainter & painter);
        void printFeatures(QPainter & painter);
        void drawLatLonGrid(QPainter & painter);
        void drawDownloadAreas(QPainter & painter);
        void drawScale(QPainter & painter);

        void panScreen(QPoint delta) ;
        void invalidate(bool updateStaticBuffer, bool updateMap);

        virtual void paintEvent(QPaintEvent* anEvent);
        virtual void mousePressEvent(QMouseEvent * event);
        virtual void mouseReleaseEvent(QMouseEvent * event);
        virtual void mouseMoveEvent(QMouseEvent* event);
        virtual void mouseDoubleClickEvent(QMouseEvent* event);
        virtual void wheelEvent(QWheelEvent* ev);
        virtual void resizeEvent(QResizeEvent *event);
        #ifdef GEOIMAGE
        virtual void dragEnterEvent(QDragEnterEvent *event);
        virtual void dragMoveEvent(QDragMoveEvent *event);
        virtual void dropEvent(QDropEvent *event);
        #endif // GEOIMAGE

        Projection& projection();
        QTransform& transform();
        QTransform& invertedTransform();
        QPoint toView(const Coord& aCoord) const;
        QPoint toView(Node* aPt) const;
        Coord fromView(const QPoint& aPt) const;

        PropertiesDock* properties();
        //InfoDock* info();

        bool isSelectionLocked();
        void lockSelection();
        void unlockSelection();

        void setViewport(const CoordBox& Map);
        void setViewport(const CoordBox& Map, const QRect& Screen);
        CoordBox viewport() const;
        static void transformCalc(QTransform& theTransform, const Projection& theProjection, const CoordBox& TargetMap, const QRect& Screen);
        double pixelPerM() const;
        double nodeWidth() const;

        void zoom(double d, const QPoint& Around);
        void zoom(double d, const QPoint& Around, const QRect& Screen);
        void adjustZoomToBoris();
        void setCenter(Coord& Center, const QRect& Screen);
        void resize(QSize oldS, QSize newS);

        bool toXML(QDomElement xParent);
        void fromXML(const QDomElement e);

        RendererOptions renderOptions();
        void setRenderOptions(const RendererOptions& opt);

        QString toPropertiesHtml();

    private:
        void drawGPS(QPainter & painter);
        void updateStaticBackground();
        void updateStaticBuffer();

        MainWindow* Main;
        Projection theProjection;
        Document* theDocument;
        Interaction* theInteraction;
        QPixmap* StaticBackground;
        QPixmap* StaticBuffer;
        QPixmap* StaticMap;
        bool StaticBufferUpToDate;
        bool StaticMapUpToDate;
        bool SelectionLocked;
        QLabel* lockIcon;
        QList<Feature*> theSnapList;

        void viewportRecalc(const QRect& Screen);

        #ifdef GEOIMAGE
        Node *dropTarget;
        #endif

        int numImages;

        QShortcut* MoveLeftShortcut;
        QShortcut* MoveRightShortcut;
        QShortcut* MoveUpShortcut;
        QShortcut* MoveDownShortcut;
        QShortcut* ZoomInShortcut;
        QShortcut* ZoomOutShortcut;

    public slots:
        virtual void on_MoveLeft_activated();
        virtual void on_MoveRight_activated();
        virtual void on_MoveUp_activated();
        virtual void on_MoveDown_activated();
        virtual void on_ZoomIn_activated();
        virtual void on_ZoomOut_activated();

    signals:
        void interactionChanged(Interaction* anInteraction);
        void viewportChanged();

    protected:
        bool event(QEvent *event);

    private slots:
        void on_imageRequested(ImageMapLayer*);
        void on_imageReceived(ImageMapLayer*);
        void on_loadingFinished(ImageMapLayer*);
        void on_customContextMenuRequested(const QPoint & pos);

    private:
        MapViewPrivate* p;
};

#endif


