import canopen

# Connexion au bus CAN via socketcan
network = canopen.Network()


network.connect(channel='can0', bustype='socketcan', bitrate=500000)

node = network.add_node(0, 'sample.eds')


network.disconnect()
