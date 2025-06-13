import canopen

# Connexion au bus CAN via socketcan
network = canopen.Network()
network.connect(channel='can0', bustype='socketcan', bitrate=500000)

# Ajout du nœud distant avec EDS
node = canopen.RemoteNode(0, 'bluepill.eds')
node.id = 0  # ⚠️ Important si l'EDS n'a pas NodeID
network.add_node(node)

try:
    # Lecture de l'objet 0x2000 subindex 0
    val = node.sdo[0x2000][0].raw
    print(f"Valeur lue à 0x2000:0 = {val}")
finally:
    # Déconnexion propre du bus CAN
    network.disconnect()
