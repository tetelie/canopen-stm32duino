import canopen

# Connexion au bus CAN via socketcan
network = canopen.Network()


network.connect(channel='can0', bustype='socketcan', bitrate=500000)

node = network.add_node(0, 'stm32.eds')

for node_id in network:
	print("node = ", network[node_id])

network.disconnect()
