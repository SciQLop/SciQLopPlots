from PySide6.QtCore import Qt
from SciQLopPlots import SciQLopPlot
from PySide6.QtGui import QPainter, QPen, QImage, QIcon
from PySide6.QtWidgets import QApplication, QWidget, QStyledItemDelegate, QComboBox, QVBoxLayout
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow


os.environ['QT_API'] = 'PySide6'


class DrawingAPITester(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.setWindowTitle("Drawing API Tester")
        self.resize(400, 400)

    def paintEvent(self, event):
        painter = QPainter(self)
        #painter.setRenderHint(QPainter.Antialiasing)

        pen = QPen(Qt.black)
        pen.setWidth(2)
        painter.setPen(pen)
        painter.drawRect(100, 100, 200, 200)
        painter.translate(100, 100)
        painter.drawImage(0, 0, QImage(":/LineStyles/StepCenter.png").scaledToWidth(100*painter.device().devicePixelRatio()))
        painter.translate(100, 100)
        painter.drawImage(0, 0, QImage(":/LineStyles/StepCenter.png").scaledToWidth(100))


class CustomDelegate(QStyledItemDelegate):
    def __init__(self, parent):
        super().__init__(parent)

    def paint(self, painter, option, index):
        painter.save()
        painter.translate(option.rect.topLeft())
        text = index.data()
        print(f":/LineStyles/{text}.png")
        painter.drawImage(0, 0, QImage(f":/LineStyles/{text}.png").scaledToHeight(option.rect.height()/painter.device().devicePixelRatio()))
        painter.restore()


class TestCombox(QWidget):

    def __init__(self, parent=None):
        super().__init__(parent)

        self._layout = QVBoxLayout()
        self.setLayout(self._layout)
        self._combo = QComboBox()
        self._layout.addStretch()
        self._layout.addWidget(self._combo)
        self._layout.addStretch()
        for i in ("Line", "NoLine", "StepCenter", "StepLeft", "StepRight"):
            self._combo.addItem(i)
            #self._combo.addItem(QIcon(f":/LineStyles/{i}.png"), i)
        self._combo.setItemDelegate(CustomDelegate(self))



if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(DrawingAPITester(), "Drawing API Tester")
    w.add_tab(TestCombox(), "Test Combox")
    w.show()
    app.exec()
