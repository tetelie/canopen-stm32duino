import canopen

def pdo_handler(pdo):
	try :
		val = node.rpdo[1]['objet_test'].raw
		print("PDO reçu", val)
	except Exception as e :
		print( "Erreur ", e)
	

network = canopen.Network()
network.connect(channel='can0',bustype='socketcan')

node = canopen.RemoteNode(4,'/home/vacop/Desktop/can-open-rsp/ODs/Slave_STM32/Slave_STM32.eds')
print("Affichage Node",node)
network.add_node(node)
node.nmt.state = 'PRE-OPERATIONAL'

print("Verif config eds")
# ~ for idx, obj in node.object_dictionary.items():
	# ~ print(f" -{hex(idx)} {obj.name}")
	# ~ if hasattr(obj, "subindices"):
		# ~ for sub in obj.subindices.values():
			# ~ print(f"	sub {hex(sub.subindex)}: {sub.value}")
	# ~ else :
		# ~ print(" Variable sans subindex")
		
print("Lecture TPDO ")

rpdo = node.rpdo[1]
rpdo.clear()
# ~ node.rpdo.read()
rpdo.add_variable(0x2110,0x01)
rpdo.cob_id = 0x180
rpdo.enabled= True
rpdo.add_callback(pdo_handler)

node.nmt.state = 'OPERATIONAL'
print("Passage op")

# ~ for idx, obj in node.object_dictionary.items():
	# ~ print(f" -{hex(idx)} {obj.name}")
	# ~ if hasattr(obj, "subindices"):
		# ~ for sub in obj.subindices.values():
			# ~ print(f"	sub {hex(sub.subindex)}: {sub.value}")
	# ~ else :
		# ~ print(" Variable sans subindex")
		
print("attente")

while True:
	pass
# ~ device_name = node.sdo[0x1001].raw
# ~ print("Nom de l'appareil :", device_name)

# node.sdo[0x2000].row = 42

network.disconnect()
