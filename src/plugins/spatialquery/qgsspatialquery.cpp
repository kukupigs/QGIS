/***************************************************************************
                          qgsspatialquery.cpp
                             -------------------
    begin                : Dec 29, 2009
    copyright            : (C) 2009 by Diego Moreira And Luiz Motta
    email                : moreira.geo at gmail.com And motta.luiz at gmail.com

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QMessageBox>

#include "qgsvectordataprovider.h"
#include "qgsfeature.h"
#include "qgsgeometrycoordinatetransform.h"
#include "qgsgeometryengine.h"
#include "qgsspatialquery.h"

QgsSpatialQuery::QgsSpatialQuery( MngProgressBar *pb )
    : mPb( pb )
    , mReaderFeaturesTarget( nullptr )
    , mLayerTarget( nullptr )
    , mLayerReference( nullptr )
{
  mUseTargetSelection = mUseReferenceSelection = false;
} // QgsSpatialQuery::QgsSpatialQuery(MngProgressBar *pb)

QgsSpatialQuery::~QgsSpatialQuery()
{
  delete mReaderFeaturesTarget;

} // QgsSpatialQuery::~QgsSpatialQuery()

void QgsSpatialQuery::setSelectedFeaturesTarget( bool useSelected )
{
  mUseTargetSelection = useSelected;

} // void QgsSpatialQuery::setSelectedFeaturesTarget(bool useSelected)

void QgsSpatialQuery::setSelectedFeaturesReference( bool useSelected )
{
  mUseReferenceSelection = useSelected;

} // void QgsSpatialQuery::setSelectedFeaturesReference(bool useSelected)

void QgsSpatialQuery::runQuery( QgsFeatureIds &qsetIndexResult,
                                QgsFeatureIds &qsetIndexInvalidTarget,
                                QgsFeatureIds &qsetIndexInvalidReference,
                                int relation, QgsVectorLayer* lyrTarget, QgsVectorLayer* lyrReference )
{
  setQuery( lyrTarget, lyrReference );

  // Create Spatial index for Reference - Set mIndexReference
  mPb->setFormat( QObject::tr( "Processing 1/2 - %p%" ) );
  int totalStep = mUseReferenceSelection
                  ? mLayerReference->selectedFeatureCount()
                  : ( int )( mLayerReference->featureCount() );
  mPb->init( 1, totalStep );
  setSpatialIndexReference( qsetIndexInvalidReference ); // Need set mLayerReference before

  // Make Query
  mPb->setFormat( QObject::tr( "Processing 2/2 - %p%" ) );
  totalStep = mUseTargetSelection
              ? mLayerTarget->selectedFeatureCount()
              : ( int )( mLayerTarget->featureCount() );
  mPb->init( 1, totalStep );

  execQuery( qsetIndexResult, qsetIndexInvalidTarget, relation );

} // QSet<int> QgsSpatialQuery::runQuery( int relation)

QMap<QString, int>* QgsSpatialQuery::getTypesOperations( QgsVectorLayer* lyrTarget, QgsVectorLayer* lyrReference )
{
  /* Relations from OGC document (obtain in February 2010)
     06-103r3_Candidate_Implementation_Specification_for_Geographic_Information_-_Simple_feature_access_-_Part_1_Common_Architecture_v1.2.0.pdf

     (P)oint, (L)ine and (A)rea
     Target Geometry(P,L,A) / Reference Geometry (P,L,A)
     dim -> Dimension of geometry
     Relations:
      1) Intersects and Disjoint: All
      2) Touches: P/L P/A L/L L/A A/A
         dimReference  > dimTarget OR dimReference = dimTarget if dimReference > 0
      3) Crosses: P/L P/A L/L L/A
         dimReference  > dimTarget OR dimReference = dimTarget if dimReference = 1
      4) Within: P/L P/A L/A A/A
         dimReference  > dimTarget OR dimReference = dimTarget if dimReference = 2
      5) Equals: P/P L/L A/A
         dimReference = dimTarget
      6) Overlaps: P/P L/L A/A
         dimReference = dimTarget
      7) Contains: L/P A/P A/L A/A
         dimReference  <  dimTarget OR dimReference = dimTarget if dimReference = 2
  */

  QMap<QString, int> * operations = new QMap<QString, int>;
  operations->insert( QObject::tr( "Intersects" ), Intersects );
  operations->insert( QObject::tr( "Is disjoint" ), Disjoint );

  short int dimTarget = 0, dimReference = 0;
  dimTarget = dimensionGeometry( lyrTarget->geometryType() );
  dimReference = dimensionGeometry( lyrReference->geometryType() );

  if ( dimReference > dimTarget )
  {
    operations->insert( QObject::tr( "Touches" ), Touches );
    operations->insert( QObject::tr( "Crosses" ), Crosses );
    operations->insert( QObject::tr( "Within" ), Within );
  }
  else if ( dimReference < dimTarget )
  {
    operations->insert( QObject::tr( "Contains" ), Contains );
  }
  else // dimReference == dimTarget
  {
    operations->insert( QObject::tr( "Equals" ), Equals );
    operations->insert( QObject::tr( "Overlaps" ), Overlaps );
    switch ( dimReference )
    {
      case 0:
        break;
      case 1:
        operations->insert( QObject::tr( "Touches" ), Touches );
        operations->insert( QObject::tr( "Crosses" ), Crosses );
        break;
      case 2:
        operations->insert( QObject::tr( "Touches" ), Touches );
        operations->insert( QObject::tr( "Within" ), Within );
        operations->insert( QObject::tr( "Contains" ), Contains );
    }
  }
  return operations;

} // QMap<QString, int>* QgsSpatialQuery::getTypesOperators(QgsVectorLayer* lyrTarget, QgsVectorLayer* lyrReference)

short int QgsSpatialQuery::dimensionGeometry( Qgis::GeometryType geomType )
{
  int dimGeom = 0;
  switch ( geomType )
  {
    case Qgis::Point:
      dimGeom = 0;
      break;
    case Qgis::Line:
      dimGeom = 1;
      break;
    case Qgis::Polygon:
      dimGeom = 2;
      break;
    default:
      Q_ASSERT( 0 );
      break;
  }
  return dimGeom;

} // int QgsSpatialQuery::dimensionGeometry(Qgis::GeometryType geomType)

void QgsSpatialQuery::setQuery( QgsVectorLayer *layerTarget, QgsVectorLayer *layerReference )
{
  mLayerTarget = layerTarget;
  mReaderFeaturesTarget = new QgsReaderFeatures( mLayerTarget, mUseTargetSelection );
  mLayerReference = layerReference;

} // void QgsSpatialQuery::setQuery (QgsVectorLayer *layerTarget, QgsVectorLayer *layerReference)

bool QgsSpatialQuery::hasValidGeometry( QgsFeature &feature )
{
  if ( !feature.isValid() )
    return false;

  const QgsGeometry *geom = feature.constGeometry();

  if ( !geom || geom->isGeosEmpty() )
    return false;

  return true;

} // bool QgsSpatialQuery::hasValidGeometry(QgsFeature &feature)

void QgsSpatialQuery::setSpatialIndexReference( QgsFeatureIds &qsetIndexInvalidReference )
{
  QgsReaderFeatures * readerFeaturesReference = new QgsReaderFeatures( mLayerReference, mUseReferenceSelection );
  QgsFeature feature;
  int step = 1;
  while ( readerFeaturesReference->nextFeature( feature ) )
  {
    mPb->step( step++ );

    if ( !hasValidGeometry( feature ) )
    {
      qsetIndexInvalidReference.insert( feature.id() );
      continue;
    }

    mIndexReference.insertFeature( feature );
  }
  delete readerFeaturesReference;

} // void QgsSpatialQuery::setSpatialIndexReference()

void QgsSpatialQuery::execQuery( QgsFeatureIds &qsetIndexResult, QgsFeatureIds &qsetIndexInvalidTarget, int relation )
{
  bool ( QgsGeometryEngine::* operation )( const QgsAbstractGeometryV2&, QString* ) const;
  switch ( relation )
  {
    case Disjoint:
      operation = &QgsGeometryEngine::disjoint;
      break;
    case Equals:
      operation = &QgsGeometryEngine::isEqual;
      break;
    case Touches:
      operation = &QgsGeometryEngine::touches;
      break;
    case Overlaps:
      operation = &QgsGeometryEngine::overlaps;
      break;
    case Within:
      operation = &QgsGeometryEngine::within;
      break;
    case Contains:
      operation = &QgsGeometryEngine::contains;
      break;
    case Crosses:
      operation = &QgsGeometryEngine::crosses;
      break;
    case Intersects:
      operation = &QgsGeometryEngine::intersects;
      break;
    default:
      qWarning( "undefined operation" );
      return;
  }

  // Transform referencer Target = Reference
  QgsGeometryCoordinateTransform *coordinateTransform = new QgsGeometryCoordinateTransform();
  coordinateTransform->setCoordinateTransform( mLayerTarget, mLayerReference );

  // Set function for populate result
  void ( QgsSpatialQuery::* funcPopulateIndexResult )( QgsFeatureIds&, QgsFeatureId, QgsGeometry *, bool ( QgsGeometryEngine::* )( const QgsAbstractGeometryV2&, QString* ) const );
  funcPopulateIndexResult = ( relation == Disjoint )
                            ? &QgsSpatialQuery::populateIndexResultDisjoint
                            : &QgsSpatialQuery::populateIndexResult;

  QgsFeature featureTarget;
  QgsGeometry * geomTarget;
  int step = 1;
  while ( mReaderFeaturesTarget->nextFeature( featureTarget ) )
  {
    mPb->step( step++ );

    if ( !hasValidGeometry( featureTarget ) )
    {
      qsetIndexInvalidTarget.insert( featureTarget.id() );
      continue;
    }

    geomTarget = featureTarget.geometry();
    coordinateTransform->transform( geomTarget );

    ( this->*funcPopulateIndexResult )( qsetIndexResult, featureTarget.id(), geomTarget, operation );
  }
  delete coordinateTransform;

} // QSet<int> QgsSpatialQuery::execQuery( QSet<int> & qsetIndexResult, int relation)

void QgsSpatialQuery::populateIndexResult(
  QgsFeatureIds &qsetIndexResult, QgsFeatureId idTarget, QgsGeometry * geomTarget,
  bool ( QgsGeometryEngine::* op )( const QgsAbstractGeometryV2&, QString* ) const )
{
  QgsFeatureIds listIdReference = mIndexReference.intersects( geomTarget->boundingBox() ).toSet();
  if ( listIdReference.isEmpty() )
  {
    return;
  }

  //prepare geometry
  QgsGeometryEngine* geomEngine = geomTarget->createGeometryEngine( geomTarget->geometry() );
  geomEngine->prepareGeometry();

  QgsFeature featureReference;
  const QgsGeometry * geomReference;
  QgsFeatureIterator listIt = mLayerReference->getFeatures( QgsFeatureRequest().setFilterFids( listIdReference ) );

  while ( listIt.nextFeature( featureReference ) )
  {
    geomReference = featureReference.constGeometry();

    if (( geomEngine->*op )( *( geomReference->geometry() ), 0 ) )
    {
      qsetIndexResult.insert( idTarget );
      break;
    }
  }

  delete geomEngine;
} // void QgsSpatialQuery::populateIndexResult(...

void QgsSpatialQuery::populateIndexResultDisjoint(
  QgsFeatureIds &qsetIndexResult, QgsFeatureId idTarget, QgsGeometry * geomTarget,
  bool ( QgsGeometryEngine::* op )( const QgsAbstractGeometryV2&, QString* ) const )
{
  QgsFeatureIds listIdReference = mIndexReference.intersects( geomTarget->boundingBox() ).toSet();
  if ( listIdReference.isEmpty() )
  {
    qsetIndexResult.insert( idTarget );
    return;
  }

  //prepare geometry
  QgsGeometryEngine* geomEngine = geomTarget->createGeometryEngine( geomTarget->geometry() );
  geomEngine->prepareGeometry();

  QgsFeature featureReference;
  const QgsGeometry * geomReference;
  QgsFeatureIterator listIt = mLayerReference->getFeatures( QgsFeatureRequest().setFilterFids( listIdReference ) );

  bool addIndex = true;
  while ( listIt.nextFeature( featureReference ) )
  {
    geomReference = featureReference.constGeometry();
    if (( geomEngine->*op )( *( geomReference->geometry() ), 0 ) )
    {
      addIndex = false;
      break;
    }
  }
  if ( addIndex )
  {
    qsetIndexResult.insert( idTarget );
  }
  delete geomEngine;
} // void QgsSpatialQuery::populateIndexResultDisjoint( ...

