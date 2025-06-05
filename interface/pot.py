import sys
import serial
import re
import time
from collections import deque
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QLabel, QComboBox
)
from PyQt5.QtCore import QTimer, Qt, QRect, QPointF
from PyQt5.QtGui import QPainter, QPen, QColor
from PyQt5.QtChart import QChart, QChartView, QLineSeries
import serial.tools.list_ports


def interpolate_color(value_ratio):
    if value_ratio < 0.5:
        start = QColor("#2ecc71")
        end = QColor("#f39c12")
        t = value_ratio / 0.5
    else:
        start = QColor("#f39c12")
        end = QColor("#e74c3c")
        t = (value_ratio - 0.5) / 0.5

    r = int(start.red() + (end.red() - start.red()) * t)
    g = int(start.green() + (end.green() - start.green()) * t)
    b = int(start.blue() + (end.blue() - start.blue()) * t)

    return QColor(r, g, b)


class ArcGauge(QWidget):
    def __init__(self):
        super().__init__()
        self.value = 0
        self.max_value = 4095
        self.setMinimumSize(200, 200)

    def setValue(self, value):
        value = min(max(0, value), self.max_value)
        if value != self.value:
            self.value = value
            self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        size = min(self.width(), self.height())
        side = size - 20
        x = (self.width() - side) // 2
        y = (self.height() - side) // 2
        rect = QRect(x, y, side, side)

        start_angle = 225 * 16
        span_angle = -270 * 16

        pen = QPen(QColor("#ddd"), 20)
        painter.setPen(pen)
        painter.drawArc(rect, start_angle, span_angle)

        value_ratio = self.value / self.max_value
        arc_color = interpolate_color(value_ratio)

        pen.setColor(arc_color)
        painter.setPen(pen)
        angle = int(span_angle * value_ratio)
        painter.drawArc(rect, start_angle, angle)

        painter.save()
        center = rect.center()
        painter.translate(center)
        angle_deg = 130 - (270 * value_ratio)
        painter.rotate(-angle_deg)
        pen = QPen(QColor("#e74c3c"), 4)
        painter.setPen(pen)
        painter.drawLine(0, 0, 0, -(side // 2 - 20))
        painter.restore()


class PotentiometerGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Jauge Potentiomètre (12 bits)")
        self.setGeometry(100, 100, 600, 500)

        self.label = QLabel("Valeur : 0 / 4095")
        self.label.setAlignment(Qt.AlignCenter)
        self.label.setStyleSheet("font-size: 20px;")

        self.arc_gauge = ArcGauge()
        self.port_selector = QComboBox()
        self.port_selector.addItems(self.get_serial_ports())
        self.port_selector.currentIndexChanged.connect(self.change_port)

        self.history = deque(maxlen=100)
        self.smooth_buffer = deque(maxlen=5)  # Moyenne glissante
        self.smoothing_window = 5

        self.series = QLineSeries()
        self.chart = QChart()
        self.chart.addSeries(self.series)
        self.chart.createDefaultAxes()
        self.chart.axisX().setRange(0, 100)
        self.chart.axisY().setRange(0, 4095)
        self.chart_view = QChartView(self.chart)

        layout = QVBoxLayout()
        layout.addWidget(self.port_selector)
        layout.addWidget(self.label)
        layout.addWidget(self.arc_gauge)
        layout.addWidget(self.chart_view)

        self.setLayout(layout)

        self.serial = None
        self.change_port()

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(60)

        self.last_time = time.time()

    def get_serial_ports(self):
        ports = serial.tools.list_ports.comports()
        return [p.device for p in ports]

    def change_port(self):
        selected_port = self.port_selector.currentText()
        try:
            if self.serial and self.serial.is_open:
                self.serial.close()
            self.serial = serial.Serial(selected_port, 115200, timeout=1)
            print(f"✅ Connecté à {selected_port}")
        except Exception as e:
            print(f"❌ Impossible de se connecter à {selected_port} : {e}")
            self.serial = None

    def update_chart(self, new_value):
        self.history.append(new_value)
        self.series.clear()
        for i, val in enumerate(self.history):
            self.series.append(QPointF(i, val))

    def smooth_value(self, raw_value):
        self.smooth_buffer.append(raw_value)
        return sum(self.smooth_buffer) / len(self.smooth_buffer)

    def read_serial(self):
        if self.serial and self.serial.in_waiting:
            try:
                data = self.serial.read(self.serial.in_waiting).decode(errors="ignore").splitlines()
                for line in reversed(data):
                    match = re.search(r"Potentiomètre \(12 bits\) *: *(\d+)", line)
                    if match:
                        value = int(match.group(1))
                        value = min(max(value, 0), 4095)

                        smoothed = self.smooth_value(value)
                        self.arc_gauge.setValue(smoothed)
                        self.label.setText(f"Valeur : {value} / 4095")
                        self.update_chart(value)

                        now = time.time()
                        print(f"↪️ {value} reçu ({int((now - self.last_time)*1000)} ms entre lectures)")
                        self.last_time = now
                        break
            except Exception as e:
                print(f"Erreur de lecture : {e}")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = PotentiometerGUI()
    window.show()
    sys.exit(app.exec_())
