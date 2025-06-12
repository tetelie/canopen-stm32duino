import can
import canopen

# Ouvre l'interface CAN (SocketCAN)
bus = can.interface.Bus(channel='can0', bustype='socketcan')

# Crée un client CANopen (esclave nœud 0)
network = canopen.Network()
network.connect(bustype='socketcan', channel='can0', bitrate=500000)

# Charge un Object Dictionary (exemple minimal, tu peux utiliser un vrai EDS file)
# Ici on crée un objet manuel, sans EDS
node = canopen.RemoteNode(0, 'stm32.eds')  # idéalement avec un fichier EDS réel
network.add_node(node)

# Demande de lecture SDO sur 0x2000:0
try:
    value = node.sdo[0x2000][0].raw
    print(f"Valeur lue à 0x2000:0 = {value}")
except canopen.SdoAbortedError as e:
    print(f"Erreur SDO: {e}")

# Ferme la connexion CAN
network.disconnect()
