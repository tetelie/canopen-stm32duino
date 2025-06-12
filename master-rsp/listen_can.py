import can

def main ():
	bus = can.interface.Bus(channel='can0', bustype='socketcan')
		
	print("En écoute  sur le bus Can0")
	try :
		while True :
			message = bus.recv()
			if message is not None:
				data = message.data.decode('ascii')
				print(f"ID : {hex(message.arbitration_id)}, Données : {data}")
	except KeyboardInterrupt:
		print("\n arret de l'écoute")
	except Exception as e:
		print(f"Erreur : {e}")
			
if __name__ == "__main__":
	main()
