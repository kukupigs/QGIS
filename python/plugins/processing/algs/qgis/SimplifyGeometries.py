# -*- coding: utf-8 -*-

"""
***************************************************************************
    SimplifyGeometries.py
    ---------------------
    Date                 : August 2012
    Copyright            : (C) 2012 by Victor Olaya
    Email                : volayaf at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Victor Olaya'
__date__ = 'August 2012'
__copyright__ = '(C) 2012, Victor Olaya'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

import os

from qgis.PyQt.QtGui import QIcon

from qgis.core import Qgis, QgsFeature, QgsGeometry

from processing.core.GeoAlgorithm import GeoAlgorithm
from processing.core.ProcessingLog import ProcessingLog
from processing.core.parameters import ParameterVector
from processing.core.parameters import ParameterNumber
from processing.core.outputs import OutputVector
from processing.tools import dataobjects, vector

pluginPath = os.path.split(os.path.split(os.path.dirname(__file__))[0])[0]


class SimplifyGeometries(GeoAlgorithm):

    INPUT = 'INPUT'
    TOLERANCE = 'TOLERANCE'
    OUTPUT = 'OUTPUT'

    def getIcon(self):
        return QIcon(os.path.join(pluginPath, 'images', 'ftools', 'simplify.png'))

    def defineCharacteristics(self):
        self.name, self.i18n_name = self.trAlgorithm('Simplify geometries')
        self.group, self.i18n_group = self.trAlgorithm('Vector geometry tools')

        self.addParameter(ParameterVector(self.INPUT,
                                          self.tr('Input layer'),
                                          [ParameterVector.VECTOR_TYPE_POLYGON, ParameterVector.VECTOR_TYPE_LINE]))
        self.addParameter(ParameterNumber(self.TOLERANCE,
                                          self.tr('Tolerance'), 0.0, 10000000.0, 1.0))

        self.addOutput(OutputVector(self.OUTPUT, self.tr('Simplified')))

    def processAlgorithm(self, progress):
        layer = dataobjects.getObjectFromUri(self.getParameterValue(self.INPUT))
        tolerance = self.getParameterValue(self.TOLERANCE)

        pointsBefore = 0
        pointsAfter = 0

        writer = self.getOutputFromName(self.OUTPUT).getVectorWriter(
            layer.pendingFields().toList(), layer.wkbType(), layer.crs())

        features = vector.features(layer)
        total = 100.0 / len(features)
        for current, f in enumerate(features):
            featGeometry = QgsGeometry(f.geometry())
            attrs = f.attributes()
            pointsBefore += self.geomVertexCount(featGeometry)
            newGeometry = featGeometry.simplify(tolerance)
            pointsAfter += self.geomVertexCount(newGeometry)
            feature = QgsFeature()
            feature.setGeometry(newGeometry)
            feature.setAttributes(attrs)
            writer.addFeature(feature)
            progress.setPercentage(int(current * total))

        del writer

        ProcessingLog.addToLog(ProcessingLog.LOG_INFO,
                               self.tr('Simplify: Input geometries have been simplified from %s to %s points' % (pointsBefore, pointsAfter)))

    def geomVertexCount(self, geometry):
        geomType = geometry.type()

        if geomType == Qgis.Line:
            if geometry.isMultipart():
                pointsList = geometry.asMultiPolyline()
                points = sum(pointsList, [])
            else:
                points = geometry.asPolyline()
            return len(points)
        elif geomType == Qgis.Polygon:
            if geometry.isMultipart():
                polylinesList = geometry.asMultiPolygon()
                polylines = sum(polylinesList, [])
            else:
                polylines = geometry.asPolygon()

            points = []
            for l in polylines:
                points.extend(l)

            return len(points)
        else:
            return None
