#include <QInputDialog>
#include "QGCMapWidget.h"
#include "QGCMapToolBar.h"
#include "MAV2DIcon.h"
#include "Waypoint2DIcon.h"


QGCMapWidget::QGCMapWidget(QWidget *parent) :
    mapcontrol::OPMapWidget(parent),
    maxUpdateInterval(2.1f), // 2 seconds
    followUAVEnabled(false),
    trailType(mapcontrol::UAVTrailType::ByTimeElapsed),
    trailInterval(2.0f),
    followUAVID(0),
    mapInitialized(false),
    mapPositionInitialized(false),
    homeAltitude(0),
    zoomBlocked(false)
{

    offlineMode = true;
    // Widget is inactive until shown
    defaultGuidedAlt = -1;
    loadSettings(false);

    //handy for debugging:
    //this->SetShowTileGridLines(true);

    //default appears to be Google Hybrid, and is broken currently
#if defined MAP_DEFAULT_TYPE_BING
    this->SetMapType(MapType::BingHybrid);
#elif defined MAP_DEFAULT_TYPE_GOOGLE
    this->SetMapType(MapType::GoogleHybrid);
#else
    this->SetMapType(MapType::OpenStreetMap);
#endif

    this->setContextMenuPolicy(Qt::ActionsContextMenu);

    // Go to options
    QAction *guidedaction = new QAction(this);
    guidedaction->setText("Go To Here (Guided Mode)");
    connect(guidedaction,SIGNAL(triggered()),this,SLOT(guidedActionTriggered()));
    this->addAction(guidedaction);
    guidedaction = new QAction(this);
    guidedaction->setText("Go To Here Alt (Guided Mode)");
    connect(guidedaction,SIGNAL(triggered()),this,SLOT(guidedAltActionTriggered()));
    this->addAction(guidedaction);
    // Point camera option
    QAction *cameraaction = new QAction(this);
    cameraaction->setText("Point Camera Here");
    connect(cameraaction,SIGNAL(triggered()),this,SLOT(cameraActionTriggered()));
    this->addAction(cameraaction);
    // Set home location option
    QAction *sethomeaction = new QAction(this);
    sethomeaction->setText("Set Home Location Here");
    connect(sethomeaction,SIGNAL(triggered()),this,SLOT(setHomeActionTriggered()));
    this->addAction(sethomeaction);
}
void QGCMapWidget::guidedActionTriggered()
{

}
bool QGCMapWidget::guidedAltActionTriggered()
{

    bool ok = false;
    int tmpalt = QInputDialog::getInt(this,"Altitude","Enter default altitude (in meters) of destination point for guided mode",100,0,30000,1,&ok);
    if (!ok)
    {
        //Use has chosen cancel. Do not send the waypoint
        return false;
    }
    defaultGuidedAlt = tmpalt;
    guidedActionTriggered();
    return true;
}
void QGCMapWidget::cameraActionTriggered()
{


}

/**
 * @brief QGCMapWidget::setHomeActionTriggered
 */
bool QGCMapWidget::setHomeActionTriggered()
{


    // Enter an altitude
    bool ok = false;
    double alt = QInputDialog::getDouble(this,"Home Altitude","Enter altitude (in meters) of new home location",0.0,0.0,30000.0,2,&ok);
    if (!ok) return false; //Use has chosen cancel. Do not send the waypoint

    // Create new waypoint and send it to the WPManager to send out.
    internals::PointLatLng pos = map->FromLocalToLatLng(contextMousePressPos.x(), contextMousePressPos.y());
    qDebug("Set home location sent. Lat: %f, Lon: %f, Alt: %f.", pos.Lat(), pos.Lng(), alt);


    return true;
}

void QGCMapWidget::mousePressEvent(QMouseEvent *event)
{

    // Store right-click event presses separate for context menu
    // TODO add check if click was on map, or popup box.
    if (event->button() == Qt::RightButton) {
        contextMousePressPos = event->pos();
    }

    mapcontrol::OPMapWidget::mousePressEvent(event);
}

void QGCMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    mousePressPos = event->pos();
    mapcontrol::OPMapWidget::mouseReleaseEvent(event);


}

QGCMapWidget::~QGCMapWidget()
{
    SetShowHome(false);	// doing this appears to stop the map lib crashing on exit
    SetShowUAV(false);	//   "          "
    storeSettings();
}

void QGCMapWidget::showEvent(QShowEvent* event)
{
    // Disable OP's standard UAV, we have more than one
    SetShowUAV(false);

    // Pass on to parent widget
    OPMapWidget::showEvent(event);

    // Connect map updates to the adapter slots
    connect(this, SIGNAL(WPValuesChanged(WayPointItem*)), this, SLOT(handleMapWaypointEdit(WayPointItem*)));




    if (!mapInitialized)
    {
        internals::PointLatLng pos_lat_lon = internals::PointLatLng(0, 0);

        SetMouseWheelZoomType(internals::MouseWheelZoomType::MousePositionWithoutCenter);	    // set how the mouse wheel zoom functions
        SetFollowMouse(true);				    // we want a contiuous mouse position reading

        SetShowHome(true);					    // display the HOME position on the map
        Home->SetSafeArea(0);                         // set radius (meters)
        Home->SetShowSafeArea(false);                                         // show the safe area
        Home->SetCoord(pos_lat_lon);             // set the HOME position

        setFrameStyle(QFrame::NoFrame);      // no border frame
        setBackgroundBrush(QBrush(Qt::black)); // tile background

        // Start timer
        connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateGlobalPosition()));
        mapInitialized = true;
        //QTimer::singleShot(800, this, SLOT(loadSettings()));
    }
    updateTimer.start(maxUpdateInterval*1000);
    // Update all UAV positions
    updateGlobalPosition();
}

void QGCMapWidget::hideEvent(QHideEvent* event)
{
    updateTimer.stop();
    storeSettings();
    OPMapWidget::hideEvent(event);
}

void QGCMapWidget::wheelEvent ( QWheelEvent * event )
{
    if (!zoomBlocked) {
        OPMapWidget::wheelEvent(event);
    }
}

/**
 * @param changePosition Load also the last position from settings and update the map position.
 */
void QGCMapWidget::loadSettings(bool changePosition)
{
    // Atlantic Ocean near Africa, coordinate origin
    double lastZoom = 1;
    double lastLat = 0;
    double lastLon = 0;

    QSettings settings;
    settings.beginGroup("QGC_MAPWIDGET");
    if (changePosition)
    {
        lastLat = settings.value("LAST_LATITUDE", lastLat).toDouble();
        lastLon = settings.value("LAST_LONGITUDE", lastLon).toDouble();
        lastZoom = settings.value("LAST_ZOOM", lastZoom).toDouble();
    }
    trailType = static_cast<mapcontrol::UAVTrailType::Types>(settings.value("TRAIL_TYPE", trailType).toInt());
    trailInterval = settings.value("TRAIL_INTERVAL", trailInterval).toFloat();
    settings.endGroup();

    // SET CORRECT MENU CHECKBOXES
    // Set the correct trail interval
    if (trailType == mapcontrol::UAVTrailType::ByDistance)
    {
        // XXX
        qDebug() << "WARNING: Settings loading for trail type (ByDistance) not implemented";
    }
    else if (trailType == mapcontrol::UAVTrailType::ByTimeElapsed)
    {
        // XXX
        qDebug() << "WARNING: Settings loading for trail type (ByTimeElapsed) not implemented";
    }

    // SET TRAIL TYPE
    foreach (mapcontrol::UAVItem* uav, GetUAVS())
    {
        // Set the correct trail type
        uav->SetTrailType(trailType);
        // Set the correct trail interval
        if (trailType == mapcontrol::UAVTrailType::ByDistance)
        {
            uav->SetTrailDistance(trailInterval);
        }
        else if (trailType == mapcontrol::UAVTrailType::ByTimeElapsed)
        {
            uav->SetTrailTime(trailInterval);
        }
    }

    // SET INITIAL POSITION AND ZOOM
    internals::PointLatLng pos_lat_lon = internals::PointLatLng(lastLat, lastLon);
    SetCurrentPosition(pos_lat_lon);        // set the map position
    SetZoom(lastZoom); // set map zoom level
}

void QGCMapWidget::storeSettings()
{
    QSettings settings;
    settings.beginGroup("QGC_MAPWIDGET");
    internals::PointLatLng pos = CurrentPosition();
    settings.setValue("LAST_LATITUDE", pos.Lat());
    settings.setValue("LAST_LONGITUDE", pos.Lng());
    settings.setValue("LAST_ZOOM", ZoomReal());
    settings.setValue("TRAIL_TYPE", static_cast<int>(trailType));
    settings.setValue("TRAIL_INTERVAL", trailInterval);
    settings.endGroup();
    settings.sync();
}

void QGCMapWidget::mouseDoubleClickEvent(QMouseEvent* event)
{


    OPMapWidget::mouseDoubleClickEvent(event);
}




/**
 * Pulls in the positions of all UAVs from the UAS manager
 */
void QGCMapWidget::updateGlobalPosition()
{


}

void QGCMapWidget::updateLocalPosition()
{


}

void QGCMapWidget::updateLocalPositionEstimates()
{
    updateLocalPosition();
}


void QGCMapWidget::updateSystemSpecs(int uas)
{
    foreach (mapcontrol::UAVItem* p, UAVS.values())
    {
        MAV2DIcon* icon = dynamic_cast<MAV2DIcon*>(p);
        if (icon && icon->getUASId() == uas)
        {
           }
    }
}




// MAP NAVIGATION
void QGCMapWidget::showGoToDialog()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Please enter coordinates"),
                                         tr("Coordinates (Lat,Lon):"), QLineEdit::Normal,
                                         QString("%1,%2").arg(CurrentPosition().Lat(), 0, 'g', 6).arg(CurrentPosition().Lng(), 0, 'g', 6), &ok);
    if (ok && !text.isEmpty())
    {
        QStringList split = text.split(",");
        if (split.length() == 2)
        {
            bool convert;
            double latitude = split.first().toDouble(&convert);
            ok &= convert;
            double longitude = split.last().toDouble(&convert);
            ok &= convert;

            if (ok)
            {
                internals::PointLatLng pos_lat_lon = internals::PointLatLng(latitude, longitude);
                SetCurrentPosition(pos_lat_lon);        // set the map position
            }
        }
    }
}


void QGCMapWidget::updateHomePosition(double latitude, double longitude, double altitude)
{
    qDebug() << "HOME SET TO: " << latitude << longitude << altitude;
    Home->SetCoord(internals::PointLatLng(latitude, longitude));
    Home->SetAltitude(altitude);
    homeAltitude = altitude;
    SetShowHome(true);                      // display the HOME position on the map
    Home->RefreshPos();
}

void QGCMapWidget::goHome()
{
    SetCurrentPosition(Home->Coord());
    SetZoom(17);
}

/**
 * Limits the update rate on the specified interval. Set to zero (0) to run at maximum
 * telemetry speed. Recommended rate is 2 s.
 */
void QGCMapWidget::setUpdateRateLimit(float seconds)
{
    maxUpdateInterval = seconds;
    updateTimer.start(maxUpdateInterval*1000);
}

void QGCMapWidget::cacheVisibleRegion()
{
    internals::RectLatLng rect = map->SelectedArea();

    if (rect.IsEmpty())
    {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("Cannot cache tiles for offline use");
        msgBox.setInformativeText("Please select an area first by holding down SHIFT or ALT and selecting the area with the left mouse button.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
    else
    {
        RipMap();
        // Set empty area = unselect area
        map->SetSelectedArea(internals::RectLatLng());
    }
}



