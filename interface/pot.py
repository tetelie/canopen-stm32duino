import sys
import serial
import re
import time
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QLabel
from PyQt5.QtCore import QTimer, Qt, QRect
from PyQt5.QtGui import QPainter, QPen, QColor

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

        # Fond gris
        pen = QPen(QColor("#ddd"), 20)
        painter.setPen(pen)
        painter.drawArc(rect, start_angle, span_angle)

        # Valeur en bleu
        pen.setColor(QColor("#3498db"))
        painter.setPen(pen)
        angle = int(span_angle * self.value / self.max_value)
        painter.drawArc(rect, start_angle, angle)

class PotentiometerGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Jauge Potentiomètre (12 bits)")
        self.setGeometry(100, 100, 400, 300)

        layout = QVBoxLayout()

        self.label = QLabel("Valeur : 0 / 4095")
        self.label.setAlignment(Qt.AlignCenter)
        self.label.setStyleSheet("font-size: 20px;")
        layout.addWidget(self.label)

        self.arc_gauge = ArcGauge()
        layout.addWidget(self.arc_gauge)

        self.setLayout(layout)

        try:
            self.serial = serial.Serial("/dev/tty.usbserial-0001", 115200, timeout=1)
        except serial.SerialException:
            print("❌ Erreur : port série non trouvé.")
            self.serial = None

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(200)  # Toutes les 200 ms

        self.last_time = time.time()  # Pour debug vitesse

    def read_serial(self):
        if self.serial and self.serial.in_waiting:
            try:
                data = self.serial.read(self.serial.in_waiting).decode(errors="ignore").splitlines()
                for line in reversed(data):
                    match = re.search(r"Potentiomètre \(12 bits\) *: *(\d+)", line)
                    if match:
                        value = int(match.group(1))
                        value = min(max(value, 0), 4095)
                        self.arc_gauge.setValue(value)
                        self.label.setText(f"Valeur : {value} / 4095")
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
