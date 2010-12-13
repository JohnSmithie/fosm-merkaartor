#include "Layer.h"

#include "Features.h"

#include "Document.h"
#include "LayerWidget.h"

#include "DocumentCommands.h"
#include "FeatureCommands.h"
#include "WayCommands.h"

#include "Utils/LineF.h"

#include <QApplication>
#include <QMultiMap>
#include <QProgressDialog>
#include <QUuid>
#include <QMap>
#include <QList>
#include <QMenu>

#include <RTree.h>

/* Layer */

typedef RTree<MapFeaturePtr, double, 2, double> MyRTree;

class LayerPrivate
{
public:
    LayerPrivate()
    {
        theDocument = NULL;
        selected = false;
        Enabled = true;
        Readonly = false;
        Uploadable = true;

        IndexingBlocked = false;
        VirtualsUpdatesBlocked = false;
    }
    ~LayerPrivate()
    {
        foreach (MapFeaturePtr f, Features)
            delete f;
    }
    QList<MapFeaturePtr> Features;
    MyRTree theRTree;

    QHash<qint64, MapFeaturePtr> IdMap;
    QString Name;
    QString Description;
    bool Visible;
    bool selected;
    bool Enabled;
    bool Readonly;
    bool Uploadable;
    bool IndexingBlocked;
    bool VirtualsUpdatesBlocked;
    qreal alpha;
    int dirtyLevel;

    Document* theDocument;
};

Layer::Layer()
:  p(new LayerPrivate), theWidget(0)
{
    p->alpha = 1.0;
    p->dirtyLevel = 0;
}

Layer::Layer(const QString& aName)
:  p(new LayerPrivate), theWidget(0)
{
    p->Name = aName;
    p->alpha = 1.0;
    p->dirtyLevel = 0;
}

Layer::Layer(const Layer&)
: QObject(), p(0), theWidget(0)
{
}

Layer::~Layer()
{
    SAFE_DELETE(p);
}

struct IndexFindContext {
    QMap<RenderPriority, QSet <Feature*> >* theFeatures;
    QRectF* clipRect;
    Projection* theProjection;
    QTransform* theTransform;
    bool arePointsDrawable;
};

bool __cdecl indexFindCallbackList(MapFeaturePtr F, void* ctxt)
{
    ((QList<MapFeaturePtr>*)(ctxt))->append(F);
    return true;
}

bool __cdecl indexFindCallback(Feature* F, void* ctxt)
{
    if (F->isHidden())
        return true;

    IndexFindContext* pCtxt = (IndexFindContext*)ctxt;

    if (pCtxt->theFeatures->value(F->renderPriority()).contains(F))
        return true;

    if (CHECK_WAY(F)) {
        Way * R = STATIC_CAST_WAY(F);
        R->buildPath(*(pCtxt->theProjection), *(pCtxt->theTransform), *(pCtxt->clipRect));
        (*(pCtxt->theFeatures))[F->renderPriority()].insert(F);
    } else
    if (CHECK_RELATION(F)) {
        Relation * RR = STATIC_CAST_RELATION(F);
        RR->buildPath(*(pCtxt->theProjection), *(pCtxt->theTransform), *(pCtxt->clipRect));
        (*(pCtxt->theFeatures))[F->renderPriority()].insert(F);
    } else
    if (F->getType() == IFeature::Point) {
        if (pCtxt->arePointsDrawable)
            if (!(F->isVirtual() && !M_PREFS->getVirtualNodesVisible()))
                (*(pCtxt->theFeatures))[F->renderPriority()].insert(F);
    } else
        (*(pCtxt->theFeatures))[F->renderPriority()].insert(F);

    return true;
}

void Layer::get(const CoordBox& bb, QList<Feature*>& theFeatures)
{
    double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
    double max[] = {bb.topRight().lon(), bb.topRight().lat()};
    p->theRTree.Search(min, max, &indexFindCallback, (void*)(&theFeatures));
}

bool getFeatureSetCallback(MapFeaturePtr /*data*/, void* /*ctxt*/)
{
//    if (theFeatures[(*it)->renderPriority()].contains(*it))
//        continue;
//
//    if (Way * R = CAST_WAY(*it)) {
//        R->buildPath(theProjection, theTransform, clipRect);
//        theFeatures[(*it)->renderPriority()].insert(*it);
//
//    } else
//    if (Relation * RR = CAST_RELATION(*it)) {
//        RR->buildPath(theProjection, theTransform, clipRect);
//        theFeatures[(*it)->renderPriority()].insert(*it);
//    } else
//    if (Node * pt = CAST_NODE(*it)) {
//        if (arePointsDrawable())
//            theFeatures[(*it)->renderPriority()].insert(*it);
//    } else
//        theFeatures[(*it)->renderPriority()].insert(*it);
//
    return true;
}

void Layer::getFeatureSet(QMap<RenderPriority, QSet <Feature*> >& theFeatures, Document* /* theDocument */,
                   QList<CoordBox>& invalidRects, QRectF& clipRect, Projection& theProjection, QTransform& theTransform)
{
    if (!size())
        return;

    IndexFindContext ctxt;
    ctxt.theFeatures = &theFeatures;
    ctxt.clipRect = &clipRect;
    ctxt.theProjection = &theProjection;
    ctxt.theTransform = &theTransform;
    ctxt.arePointsDrawable = arePointsDrawable();

    for (int i=0; i < invalidRects.size(); ++i) {
        indexFind(invalidRects[i], ctxt);
    }
}


void Layer::setName(const QString& s)
{
    p->Name = s;
    if (theWidget) {
        theWidget->setName(s);
        theWidget->getAssociatedMenu()->setTitle(s);
    }
}

const QString& Layer::name() const
{
    return p->Name;
}

void Layer::setDescription(const QString& s)
{
    p->Description = s;
}

const QString& Layer::description() const
{
    return p->Description;
}

void Layer::setVisible(bool b) {
    p->Visible = b;
    if (theWidget) {
        theWidget->setLayerVisible(p->Visible, false);
    }

    if (p->theDocument) {
        FeatureIterator it(p->theDocument);
        for(;!it.isEnd(); ++it) {
            it.get()->invalidateMeta();
        }
    }
}

bool Layer::isVisible() const
{
    return p->Visible;
}

void Layer::setSelected(bool b) {
    p->selected = b;
}

bool Layer::isSelected() const
{
    return p->selected;
}

void Layer::setEnabled(bool b) {
    p->Enabled = b;
    if (theWidget) {
        theWidget->setVisible(b);
        theWidget->getAssociatedMenu()->menuAction()->setVisible(b);
    }

    if (p->theDocument) {
        FeatureIterator it(p->theDocument);
        for(;!it.isEnd(); ++it) {
            it.get()->invalidateMeta();
        }
    }
}

bool Layer::isEnabled() const
{
    return p->Enabled;
}

void Layer::setReadonly(bool b) {
    p->Readonly = b;

    if (p->theDocument) {
        FeatureIterator it(p->theDocument);
        for(;!it.isEnd(); ++it) {
            it.get()->invalidateMeta();
        }
    }
}

bool Layer::isReadonly() const
{
    return p->Readonly;
}

void Layer::setUploadable(bool b) {
    p->Uploadable = b;
}

bool Layer::isUploadable() const
{
    return p->Uploadable;
}

void Layer::add(Feature* aFeature)
{
    aFeature->setLayer(this);
    if (!aFeature->isDeleted() && !aFeature->isVirtual())
        indexAdd(aFeature->boundingBox(), aFeature);
    p->Features.push_back(aFeature);
    aFeature->invalidateMeta();
    notifyIdUpdate(aFeature->id(),aFeature);
}

void Layer::add(Feature* aFeature, int Idx)
{
    add(aFeature);
    std::rotate(p->Features.begin()+Idx,p->Features.end()-1,p->Features.end());
}

void Layer::notifyIdUpdate(const IFeature::FId& id, Feature* aFeature)
{
    QHash<qint64, MapFeaturePtr>::iterator i;

    if (!aFeature) {
        i = p->IdMap.find(id.numId);
        while (i != p->IdMap.end() && i.key() == id.numId) {
            if (i.value()->id().type & id.type)
                i = p->IdMap.erase(i);
            else
                ++i;
        }
    }
    else {
        if (!aFeature->isVirtual())
            p->IdMap.insertMulti(id.numId, aFeature);
    }
}

void Layer::remove(Feature* aFeature)
{
    if (p->Features.removeOne(aFeature))
    {
        aFeature->setLayer(0);
        if (!aFeature->isDeleted() && !aFeature->isVirtual())
            indexRemove(aFeature->boundingBox(), aFeature);
        notifyIdUpdate(aFeature->id(),0);
    }
}

void Layer::deleteFeature(Feature* aFeature)
{
    if (p->Features.removeOne(aFeature))
    {
        aFeature->setLayer(0);
        if (!aFeature->isDeleted() && !aFeature->isVirtual())
            indexRemove(aFeature->boundingBox(), aFeature);
        notifyIdUpdate(aFeature->id(),0);
    }

    delete aFeature;
}

void Layer::clear()
{
    while (p->Features.count())
    {
        p->Features[0]->setLayer(0);
        notifyIdUpdate(p->Features[0]->id(),0);
        p->Features.removeAt(0);
    }
    reIndex();
}

bool Layer::exists(Feature* F) const
{
    int i = p->Features.indexOf(F);
    return (i != -1);
}

int Layer::size() const
{
    return p->Features.size();
}

void Layer::setDocument(Document* aDocument)
{
    p->theDocument = aDocument;
}

Document* Layer::getDocument()
{
    return p->theDocument;
}

int Layer::get(Feature* aFeature)
{
    for (int i=0; i<p->Features.size(); ++i)
        if (p->Features[i] == aFeature)
            return i;

    return -1;
}

QList<Feature *> Layer::get()
{
    QList<Feature *> theList;
    for (int i=0; i<p->Features.size(); ++i)
        if (p->Features[i])
            theList.append(p->Features[i]);
    return theList;
}


Feature* Layer::get(int i)
{
    return p->Features.at(i);
}

Feature* Layer::get(const IFeature::FId& id)
{
    QHash<qint64, MapFeaturePtr>::const_iterator i;

    i = p->IdMap.find(id.numId);
    while (i != p->IdMap.end() && i.key() == id.numId) {
        if ((i.value()->id().type & id.type) != 0)
            return i.value();
        ++i;
    }
    return NULL;
}

const Feature* Layer::get(int i) const
{
    if((int)i>=p->Features.size()) return 0;
    return p->Features[i];
}

LayerWidget* Layer::getWidget(void)
{
    return theWidget;
}

void Layer::deleteWidget(void)
{
//	theWidget->deleteLater();
    delete theWidget;
    theWidget = NULL;
}

void Layer::setAlpha(const qreal a)
{
    p->alpha = a;

    if (p->theDocument) {
        FeatureIterator it(p->theDocument);
        for(;!it.isEnd(); ++it) {
            it.get()->invalidateMeta();
        }
    }
}

qreal Layer::getAlpha() const
{
    return p->alpha;
}

void Layer::setId(const QString& id)
{
    Id = id;
}

const QString& Layer::id() const
{
    if (Id.isEmpty())
        Id = QUuid::createUuid().toString();
    return Id;
}

void Layer::blockIndexing(bool val)
{
    p->IndexingBlocked = val;
}

bool Layer::isIndexingBlocked()
{
    return p->IndexingBlocked;
}

void Layer::indexAdd(const CoordBox& bb, const MapFeaturePtr aFeat)
{
    if (bb.isNull())
        return;
    if (!p->IndexingBlocked) {
        double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
        double max[] = {bb.topRight().lon(), bb.topRight().lat()};
        p->theRTree.Insert(min, max, aFeat);
    }
}

void Layer::indexRemove(const CoordBox& bb, const MapFeaturePtr aFeat)
{
    if (bb.isNull())
        return;
    if (!p->IndexingBlocked) {
        double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
        double max[] = {bb.topRight().lon(), bb.topRight().lat()};
        p->theRTree.Remove(min, max, aFeat);
    }
}

const QList<MapFeaturePtr>& Layer::indexFind(const CoordBox& bb)
{
    double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
    double max[] = {bb.topRight().lon(), bb.topRight().lat()};
    findResult.clear();
    p->theRTree.Search(min, max, &indexFindCallbackList, (void*)&findResult);

    return findResult;
}

void Layer::indexFind(const CoordBox& bb, const IndexFindContext& ctxt)
{
    double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
    double max[] = {bb.topRight().lon(), bb.topRight().lat()};
    p->theRTree.Search(min, max, &indexFindCallback, (void*)&ctxt);
}

void Layer::reIndex()
{
    qDebug() << "Reindexing...";

    p->theRTree.RemoveAll();

    for (int i=0; i<p->Features.size(); ++i) {
        if (p->Features.at(i)->isDeleted() || p->Features.at(i)->isVirtual())
            continue;
        Feature* f = p->Features.at(i);
        CoordBox bb = f->boundingBox();
        if (!bb.isNull()) {
            double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
            double max[] = {bb.topRight().lon(), bb.topRight().lat()};
            p->theRTree.Insert(min, max, f);
        }
    }
}

void Layer::reIndex(QProgressDialog * progress)
{
    qDebug() << "Reindexing...";

    p->theRTree.RemoveAll();

    progress->setLabelText("Indexing...");
    progress->setValue(0);
    progress->setMaximum(p->Features.size());
    for (int i=0; i<p->Features.size(); ++i) {
        if (!p->Features.at(i)->isDeleted() &&  !p->Features.at(i)->isVirtual()) {
            Feature* f = p->Features.at(i);
            CoordBox bb = f->boundingBox();
            if (!bb.isNull()) {
                double min[] = {bb.bottomLeft().lon(), bb.bottomLeft().lat()};
                double max[] = {bb.topRight().lon(), bb.topRight().lat()};
                p->theRTree.Insert(min, max, f);
            }
        }
        progress->setValue(i);
        qApp->processEvents();
    }
}

CoordBox Layer::boundingBox()
{
    if(p->Features.size()==0) return CoordBox(Coord(0,0),Coord(0,0));
    CoordBox Box;
    bool haveFirst = false;
    for (int i=0; i<p->Features.size(); ++i) {
        if (p->Features.at(i)->isDeleted())
            continue;
        if (p->Features.at(i)->notEverythingDownloaded())
            continue;
        if (haveFirst)
            Box.merge(p->Features.at(i)->boundingBox());
        else {
            Box = p->Features.at(i)->boundingBox();
            haveFirst = true;
        }
    }
    return Box;
}

int Layer::incDirtyLevel(int inc)
{
    return p->dirtyLevel += inc;
}

int Layer::decDirtyLevel(int inc)
{
    return p->dirtyLevel -= inc;
}

int Layer::setDirtyLevel(int newLevel)
{
    return (p->dirtyLevel = newLevel);
}

int Layer::getDirtyLevel() const
{
    return p->dirtyLevel;
}

int Layer::getDisplaySize() const
{
    int objects = 0;

    QList<MapFeaturePtr>::const_iterator i;
    for (i = p->Features.constBegin(); i != p->Features.constEnd(); i++) {
        if ((*i)->isVirtual())
            continue;
        ++objects;
    }

    return objects;
}

int Layer::getDirtySize() const
{
    int dirtyObjects = 0;

    QList<MapFeaturePtr>::const_iterator i;
    for (i = p->Features.constBegin(); i != p->Features.constEnd(); i++) {
        Feature* F = (*i);
        if (F->isVirtual())
            continue;
        else if (F->isDirty() && (!(F->isDeleted()) || (F->isDeleted() && F->hasOSMId())))
            ++dirtyObjects;
    }

    return dirtyObjects;
}

bool Layer::canDelete() const
{
    return (p->dirtyLevel == 0);
}

QString Layer::toMainHtml()
{
    QString desc;
    desc = QString("<big><b>%1</b></big><br/>").arg(p->Name);
    if (!p->Description.isEmpty())
        desc += QString("<b>%1</b><br/>").arg(p->Description);
    desc += QString("<small>(%1)</small>").arg(id());

    QString S =
    "<html><head/><body>"
    "<small><i>" + QString(metaObject()->className()) + "</i></small><br/>"
    + desc;
    S += "<hr/>";
    S += "<i>"+QApplication::translate("Layer", "Size")+": </i>" + QApplication::translate("Layer", "%n features", "", QCoreApplication::CodecForTr, getDisplaySize())+"<br/>";
    S += "%1";
    S += "</body></html>";

    return S;
}

QString Layer::toHtml()
{
    return toMainHtml().arg("");
}

QString Layer::toPropertiesHtml()
{
    QString h;

    h += "<u>" + p->Name + "</u><br/>";
    h += "<i>" + tr("Features") + ": </i>" + QString::number(getDirtySize());

    return h;
}

bool Layer::toXML(QDomElement& e, bool asTemplate, QProgressDialog * progress)
{
    Q_UNUSED(asTemplate);
    Q_UNUSED(progress);

    e.setAttribute("xml:id", id());
    e.setAttribute("name", p->Name);
    e.setAttribute("alpha", QString::number(p->alpha,'f',2));
    e.setAttribute("visible", QString((p->Visible ? "true" : "false")));
    e.setAttribute("selected", QString((p->selected ? "true" : "false")));
    e.setAttribute("enabled", QString((p->Enabled ? "true" : "false")));
    e.setAttribute("readonly", QString((p->Readonly ? "true" : "false")));
    e.setAttribute("uploadable", QString((p->Uploadable ? "true" : "false")));
    if (getDirtyLevel())
        e.setAttribute("dirtylevel", getDirtyLevel());

    return true;
}

Layer * Layer::fromXML(Layer* l, Document* /*d*/, const QDomElement e, QProgressDialog * /*progress*/)
{
    l->setId(e.attribute("xml:id"));
    l->setAlpha(e.attribute("alpha").toDouble());
    l->setVisible((e.attribute("visible") == "true" ? true : false));
    l->setSelected((e.attribute("selected") == "true" ? true : false));
    l->setEnabled((e.attribute("enabled") == "false" ? false : true));
    l->setReadonly((e.attribute("readonly") == "true" ? true : false));
    l->setUploadable((e.attribute("uploadable") == "false" ? false : true));
    l->setDirtyLevel((e.hasAttribute("dirtylevel") ? e.attribute("dirtylevel").toInt() : 0));

    return l;
}

// DrawingLayer

DrawingLayer::DrawingLayer(const QString & aName)
    : Layer(aName)
{
    p->Visible = true;
}

DrawingLayer::~ DrawingLayer()
{
}

LayerWidget* DrawingLayer::newWidget(void)
{
//	delete theWidget;
    theWidget = new DrawingLayerWidget(this);
    return theWidget;
}


bool DrawingLayer::toXML(QDomElement& xParent, bool asTemplate, QProgressDialog * progress)
{
    bool OK = true;

    QDomElement e = xParent.ownerDocument().createElement(metaObject()->className());
    xParent.appendChild(e);
    Layer::toXML(e, asTemplate, progress);

    if (!asTemplate) {
        QDomElement o = xParent.ownerDocument().createElement("osm");
        e.appendChild(o);
        o.setAttribute("version", "0.6");
        o.setAttribute("generator", QString("Merkaartor %1").arg(STRINGIFY(VERSION)));

        if (p->Features.size()) {
            QDomElement bb = xParent.ownerDocument().createElement("bound");
            o.appendChild(bb);
            CoordBox layBB = boundingBox();
            QString S = QString().number(coordToAng(layBB.bottomLeft().lat()),'f',6) + ",";
            S += QString().number(coordToAng(layBB.bottomLeft().lon()),'f',6) + ",";
            S += QString().number(coordToAng(layBB.topRight().lat()),'f',6) + ",";
            S += QString().number(coordToAng(layBB.topRight().lon()),'f',6);
            bb.setAttribute("box", S);
            bb.setAttribute("origin", QString("http://www.openstreetmap.org/api/%1").arg(M_PREFS->apiVersion()));
        }

        QList<MapFeaturePtr>::iterator it;
        for(it = p->Features.begin(); it != p->Features.end(); it++)
            (*it)->toXML(o, progress);

        QList<CoordBox> downloadBoxes = p->theDocument->getDownloadBoxes(this);
        if (downloadBoxes.size() && p->theDocument->getLastDownloadLayerTime().secsTo(QDateTime::currentDateTime()) < 12*3600) { // Do not export downloaded areas if older than 12h
            QDomElement downloaded = xParent.ownerDocument().createElement("DownloadedAreas");
            e.appendChild(downloaded);

            QListIterator<CoordBox>it(downloadBoxes);
            while(it.hasNext()) {
                it.next().toXML("DownloadedBoundingBox", downloaded);
            }
        }
    }

    return OK;
}

DrawingLayer * DrawingLayer::fromXML(Document* d, const QDomElement& e, QProgressDialog * progress)
{
    DrawingLayer* l = new DrawingLayer(e.attribute("name"));
    Layer::fromXML(l, d, e, progress);
    d->add(l);
    if (!DrawingLayer::doFromXML(l, d, e, progress)) {
        d->remove(l);
        delete l;
        return NULL;
    }
    return l;
}

DrawingLayer * DrawingLayer::doFromXML(DrawingLayer* l, Document* d, const QDomElement e, QProgressDialog * progress)
{
    l->blockIndexing(true);

    QDomElement c = e.firstChildElement();
    while (!c.isNull()) {
        if (c.tagName() == "osm") {
            QSet<Way*> addedWays;
            int i=0;
            QDomElement o = c.firstChildElement();
            while(!o.isNull()) {
                if (o.tagName() == "bound") {
                } else  if (o.tagName() == "way") {
                    Way* R = Way::fromXML(d, l, o);
                    if (R)
                        addedWays << R;
                    i++;
                } else if (o.tagName() == "relation") {
                    /* Relation* r = */ Relation::fromXML(d, l, o);
                    i++;
                } else  if (o.tagName() == "node") {
                    /* Node* N = */ Node::fromXML(d, l, o);
                    i++;
                } else if (o.tagName() == "trkseg") {
                    TrackSegment* T = TrackSegment::fromXML(d, l, o, progress);
                    l->add(T);
                    i++;
                }

                if (i >= progress->maximum()/100) {
                    progress->setValue(progress->value()+i);
                    i=0;
                }

                if (progress->wasCanceled())
                    break;

                o = o.nextSiblingElement();
            }

            if (i > 0) progress->setValue(progress->value()+i);
            qApp->processEvents();
        } else if (c.tagName() == "DownloadedAreas") {
            if (d->getLastDownloadLayerTime().secsTo(QDateTime::currentDateTime()) < 12*3600) {    // Do not import downloaded areas if older than 12h
                QDomElement bb = c.firstChildElement();
                while(!bb.isNull()) {
                    d->addDownloadBox(l, CoordBox::fromXML(bb));
                    bb = bb.nextSiblingElement();
                }
            }
        }
        c =c.nextSiblingElement();
    }
    l->blockIndexing(false);
    return l;
}

// TrackLayer

TrackLayer::TrackLayer(const QString & aName, const QString& filename)
    : Layer(aName), Filename(filename)
{
    p->Visible = true;
    p->Readonly = M_PREFS->getReadonlyTracksDefault();
}

TrackLayer::~ TrackLayer()
{
}

LayerWidget* TrackLayer::newWidget(void)
{
    theWidget = new TrackLayerWidget(this);
    return theWidget;
}

void TrackLayer::extractLayer()
{
    DrawingLayer* extL = new DrawingLayer(tr("Extract - %1").arg(name()));
    extL->setUploadable(false);

    Node* P;
    QList<Node*> PL;

    const double coordPer10M = (double(COORD_MAX) * 2 / 40080000) * 2;

    for (int i=0; i < size(); i++) {
        if (TrackSegment* S = dynamic_cast<TrackSegment*>(get(i))) {

            if (S->size() < 2)
                continue;

            // Cope with walking tracks
            double konstant = coordPer10M;
            double meanSpeed = S->distance() / S->duration() * 3600;
            if (meanSpeed < 10.)
                konstant /= 3.;


            PL.clear();

            P = new Node( S->getNode(0)->position() );
            P->setTime(S->getNode(0)->time());
            P->setElevation(S->getNode(0)->elevation());
            P->setSpeed(S->getNode(0)->speed());
            PL.append(P);
            int startP = 0;

            P = new Node( S->getNode(1)->position() );
            P->setTime(S->getNode(1)->time());
            P->setElevation(S->getNode(1)->elevation());
            P->setSpeed(S->getNode(1)->speed());
            PL.append(P);
            int endP = 1;

            for (int j=2; j < S->size(); j++) {
                P = new Node( S->getNode(j)->position() );
                P->setTime(S->getNode(j)->time());
                P->setElevation(S->getNode(j)->elevation());
                P->setSpeed(S->getNode(j)->speed());
                PL.append(P);
                endP = PL.size()-1;

                LineF l(toQt(PL[startP]->position()), toQt(PL[endP]->position()));
                for (int k=startP+1; k < endP; k++) {
                    double d = l.distance(toQt(PL[k]->position()));
                    if (d < konstant) {
                        Node* P = PL[k];
                        PL.removeAt(k);
                        delete P;
                        endP--;
                    } else
                        startP = k;
                }
            }

            Way* R = new Way();
            R->setLastUpdated(Feature::OSMServer);
            extL->add(R);
            for (int i=0; i < PL.size(); i++) {
                extL->add(PL[i]);
                R->add(PL[i]);
            }
        }
    }

    p->theDocument->add(extL);
}

const QString TrackLayer::getFilename()
{
    return Filename;
}

QString TrackLayer::toHtml()
{
    QString S;

    int totSegment = 0;
    int totSec = 0;
    double totDistance = 0;
    for (int i=0; i < size(); ++i) {
        if (TrackSegment* S = CAST_SEGMENT(get(i))) {
            totSegment++;
            totSec += S->duration();
            totDistance += S->distance();
        }
    }

    S += "<i>"+QApplication::translate("TrackLayer", "# of track segments")+": </i>" + QApplication::translate("TrackLayer", "%1").arg(QLocale().toString(totSegment))+"<br/>";
    S += "<i>"+QApplication::translate("TrackLayer", "Total distance")+": </i>" + QApplication::translate("TrackLayer", "%1 km").arg(QLocale().toString(totDistance, 'g', 3))+"<br/>";
    S += "<i>"+QApplication::translate("TrackLayer", "Total duration")+": </i>" + QApplication::translate("TrackLayer", "%1h %2m").arg(QLocale().toString(totSec/3600)).arg(QLocale().toString((totSec%3600)/60))+"<br/>";

    return toMainHtml().arg(S);
}

bool TrackLayer::toXML(QDomElement& xParent, bool asTemplate, QProgressDialog * progress)
{
    bool OK = true;
    if (asTemplate)
        return OK;

    QDomElement e = xParent.ownerDocument().createElement(metaObject()->className());
    xParent.appendChild(e);
    Layer::toXML(e, asTemplate, progress);

    QDomElement o = xParent.ownerDocument().createElement("gpx");
    e.appendChild(o);
    o.setAttribute("version", "1.1");
    o.setAttribute("creator", "Merkaartor");
    o.setAttribute("xmlns", "http://www.topografix.com/GPX/1/1");

    QList<Node*>	waypoints;
    QList<TrackSegment*>	segments;
    QList<MapFeaturePtr>::iterator it;
    for(it = p->Features.begin(); it != p->Features.end(); it++) {
        if (TrackSegment* S = CAST_SEGMENT(*it))
            segments.push_back(S);
        if (Node* P = CAST_NODE(*it))
            if (!P->tagValue("_waypoint_","").isEmpty())
                waypoints.push_back(P);
    }

    for (int i=0; i < waypoints.size(); ++i) {
        waypoints[i]->toGPX(o, progress);
    }

    QDomElement t = o.ownerDocument().createElement("trk");
    o.appendChild(t);

    for (int i=0; i < segments.size(); ++i)
        segments[i]->toXML(t, progress);

    return OK;
}

TrackLayer * TrackLayer::fromXML(Document* d, const QDomElement& e, QProgressDialog * progress)
{
    TrackLayer* l = new TrackLayer(e.attribute("name"));
    Layer::fromXML(l, d, e, progress);
    d->add(l);

    l->blockIndexing(true);

    QDomElement c = e.firstChildElement();
    if (c.tagName() != "gpx")
        return NULL;

    c = c.firstChildElement();
    while(!c.isNull()) {
        if (c.tagName() == "trk") {
            QDomElement t = c.firstChildElement();
            while(!t.isNull()) {
                if (t.tagName() == "trkseg") {
                    TrackSegment* N = TrackSegment::fromXML(d, l, t, progress);
                    l->add(N);
                }

                t = t.nextSiblingElement();
            }
        }
        if (c.tagName() == "wpt") {
            /* Node* N = */ Node::fromGPX(d, l, c);
            //l->add(N);
            progress->setValue(progress->value()+1);
        }

        if (progress->wasCanceled())
            break;

        c = c.nextSiblingElement();
    }

    l->blockIndexing(false);

    return l;
}

// DirtyLayer

DirtyLayer::DirtyLayer(const QString & aName)
    : DrawingLayer(aName)
{
    p->Visible = true;
}

DirtyLayer::~ DirtyLayer()
{
}

DirtyLayer* DirtyLayer::fromXML(Document* d, const QDomElement e, QProgressDialog * progress)
{
    DirtyLayer* l = new DirtyLayer(e.attribute("name"));
    Layer::fromXML(l, d, e, progress);
    d->add(l);
    d->setDirtyLayer(l);
    DrawingLayer::doFromXML(l, d, e, progress);
    return l;
}

LayerWidget* DirtyLayer::newWidget(void)
{
    theWidget = new DirtyLayerWidget(this);
    return theWidget;
}

// UploadedLayer

UploadedLayer::UploadedLayer(const QString & aName)
    : DrawingLayer(aName)
{
    p->Visible = true;
}

UploadedLayer::~ UploadedLayer()
{
}

UploadedLayer* UploadedLayer::fromXML(Document* d, const QDomElement e, QProgressDialog * progress)
{
    UploadedLayer* l = new UploadedLayer(e.attribute("name"));
    Layer::fromXML(l, d, e, progress);
    d->add(l);
    d->setUploadedLayer(l);
    DrawingLayer::doFromXML(l, d, e, progress);
    return l;
}

LayerWidget* UploadedLayer::newWidget(void)
{
    theWidget = new UploadedLayerWidget(this);
    return theWidget;
}

// DeletedLayer

DeletedLayer::DeletedLayer(const QString & aName)
    : DrawingLayer(aName)
{
    p->Visible = false;
    p->Enabled = false;
}

DeletedLayer::~ DeletedLayer()
{
}

bool DeletedLayer::toXML(QDomElement& , bool, QProgressDialog * )
{
    return true;
}

DeletedLayer* DeletedLayer::fromXML(Document* d, const QDomElement& e, QProgressDialog * progress)
{
    /* Only keep DeletedLayer for backward compatibility with MDC */
    Layer::fromXML(dynamic_cast<DrawingLayer*>(d->getDirtyOrOriginLayer()), d, e, progress);
    DrawingLayer::doFromXML(dynamic_cast<DrawingLayer*>(d->getDirtyOrOriginLayer()), d, e, progress);
    return NULL;
}

LayerWidget* DeletedLayer::newWidget(void)
{
    return NULL;
}

// FilterLayer

FilterLayer::FilterLayer(const QString& aId, const QString & aName, const QString& aFilter)
    : Layer(aName)
    , theSelectorString(aFilter)
{
    setId(aId);
    p->Visible = true;
    theSelector = TagSelector::parse(theSelectorString);
}

FilterLayer::~ FilterLayer()
{
}

void FilterLayer::setFilter(const QString& aFilter)
{
    theSelectorString = aFilter;
    delete theSelector;
    theSelector = TagSelector::parse(theSelectorString);

    FeatureIterator it(p->theDocument);
    for(;!it.isEnd(); ++it) {
        it.get()->updateFilters();
    }
}

bool FilterLayer::toXML(QDomElement& xParent, bool asTemplate, QProgressDialog * progress)
{
    QDomElement e = xParent.ownerDocument().createElement(metaObject()->className());
    xParent.appendChild(e);
    Layer::toXML(e, asTemplate, progress);
    e.setAttribute("filter", theSelectorString);

    return true;
}

FilterLayer* FilterLayer::fromXML(Document* d, const QDomElement e, QProgressDialog * progress)
{
    QString id;
    if (e.hasAttribute("xml:id"))
        id = e.attribute("xml:id");
    else
        id = QUuid::createUuid().toString();
    FilterLayer* l = new FilterLayer(id, e.attribute("name"), e.attribute("filter"));
    Layer::fromXML(l, d, e, progress);
    d->add(l);
    return l;
}

LayerWidget* FilterLayer::newWidget(void)
{
    theWidget = new FilterLayerWidget(this);
    return theWidget;
}

