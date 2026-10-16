"""A Qt 3D scene as a row of a SciQLopPlots panel.

Showcases:
- WidgetPlot: any QWidget in a panel, following the panel's time range
- A Qt3DWindow inside the panel, through QWidget.createWindowContainer
- The spacecraft drawn where it is at the middle of the time range in view
"""
import sys

import numpy as np
from PySide6.Qt3DCore import Qt3DCore
from PySide6.Qt3DExtras import Qt3DExtras
from PySide6.Qt3DRender import Qt3DRender
from PySide6.QtGui import QColor, QVector3D
from PySide6.QtWidgets import QApplication, QWidget

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, WidgetPlot

t = 1.7e9 + np.linspace(0, 12 * 3600, 20_000)           # twelve hours, seconds since 1970
phase = 2 * np.pi * (t - t[0]) / (6 * 3600)              # a six-hour orbit
position = np.column_stack([8 * np.cos(phase), 5 * np.sin(phase), 2 * np.sin(phase)])  # Earth radii

app = QApplication(sys.argv)
panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Qt 3D in a panel")
panel.set_time_axis_range(t[0], t[0] + 3600)
panel.plot(t, position, labels=["X", "Y", "Z"], plot_type=PlotType.TimeSeries)


def sphere(parent, radius, color):
    # Components parented to their entity: addComponent() does not keep them alive.
    entity = Qt3DCore.QEntity(parent)
    mesh = Qt3DExtras.QSphereMesh(entity)
    mesh.setRadius(radius)
    material = Qt3DExtras.QPhongMaterial(entity)
    material.setDiffuse(QColor(color))
    transform = Qt3DCore.QTransform(entity)
    for component in (mesh, material, transform):
        entity.addComponent(component)
    return transform


window = Qt3DExtras.Qt3DWindow()
window.defaultFrameGraph().setClearColor(QColor("black"))
root = Qt3DCore.QEntity()
sphere(root, 1.0, "royalblue")                           # the Earth
spacecraft = sphere(root, 0.5, "orange")
camera = window.camera()
camera.lens().setPerspectiveProjection(45, 16 / 9, 0.1, 1000)
camera.setPosition(QVector3D(0, -18, 8))
camera.setUpVector(QVector3D(0, 0, 1))                   # Z up, as in the plot's frame
camera.setViewCenter(QVector3D(0, 0, 0))
light = Qt3DCore.QEntity(camera)                         # a light that follows the camera
light.addComponent(Qt3DRender.QPointLight(light))
controller = Qt3DExtras.QOrbitCameraController(root)     # drag to turn around the Earth
controller.setCamera(camera)
window.setRootEntity(root)


def spacecraft_at_middle_of_view(start, stop):
    i = min(np.searchsorted(t, (start + stop) / 2), t.size - 1)
    spacecraft.setTranslation(QVector3D(*position[i]))


panel.add_plot(WidgetPlot(QWidget.createWindowContainer(window), on_time_range=spacecraft_at_middle_of_view))
panel.resize(900, 700)
panel.show()
sys.exit(app.exec())
