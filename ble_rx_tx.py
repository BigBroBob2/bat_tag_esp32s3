import simplepyble
import time
import msvcrt
#from timeit import default_timer as timer

import code
import readline
import rlcompleter
           

# These are the Nordic BLE UART Service, and the RX and TX characteristic
# UUID's. Note that we use the Peripheral RX characteristic to Write from PC to
# peripheral, and the data from the peripheral is sent via the Notify
# capability, so we do not use read() on this end.
# SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
# CHAR_periph_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
# CHAR_periph_Notify = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# Service and characteristics for ESP32 demo.
SERVICE = "0000abf0-0000-1000-8000-00805f9b34fb"
# CHAR_periph_RX = "0000abf1-0000-1000-8000-00805f9b34fb"
# CHAR_periph_Notify = "0000abf2-0000-1000-8000-00805f9b34fb"

# This is for the ESP32 Bluedroid version.
#CHAR_periph_RX = "0000abf3-0000-1000-8000-00805f9b34fb"
#CHAR_periph_Notify = "0000abf4-0000-1000-8000-00805f9b34fb"

# This is for ESP32 NimBLE version, which seems to have only one
# characteristic for read/write/notify.
CHAR_periph_RX = "0000abf1-0000-1000-8000-00805f9b34fb"
CHAR_periph_Notify = "0000abf1-0000-1000-8000-00805f9b34fb"

#_OUR_PERIPH_str = "Dale"  # Sub-string to look for in BLE peripheral ID string.
_OUR_PERIPH_str = "BatTag"  # Sub-string to look for in BLE peripheral ID string.


if __name__ != "__main__":
	quit()

adapters = simplepyble.Adapter.get_adapters()
if len(adapters) == 0:
	print("No adapters found")
	quit()

# We almost always just want the first/only Bluetooth adapter.
adapter = adapters[0]

adapter.set_callback_on_scan_start(lambda: print("Scan started."))
#adapter.set_callback_on_scan_stop(lambda: print("Scan complete."))

# This is the callback, called for each peripheral found during scanning. When
# we match to our desired ID string, we stop the scan.
def on_scan_found(peripheral):
	global periph
	print(f"Found {peripheral.identifier()} [{peripheral.address()}] \033[K")

	# Stop the scan and set "periph" when we find our peripheral.
	if _OUR_PERIPH_str in peripheral.identifier():
		periph = peripheral
		adapter.scan_stop()

adapter.set_callback_on_scan_found(on_scan_found)


# ======================================================================
# Main loop to scan and connect to peripheral.
#

# Some demo "parameters" we can set in the peripheral
param1 = 100
param2 = 2460

# Loop continuously, if disconnected, keep trying to connect. User can press 'Q'
# or ESC to set KeepGoing to False, and exit the program.
print()
KeepGoing = True
while KeepGoing:
	periph = None

	# Scan continuously until we find our desired peripheral. The BLE
	# on_scan_found() callback will stop the scan and save our "periph" if it
	# finds our desired peripheral.
	scan_count = 0
	adapter.scan_start()
	while adapter.scan_is_active():
		time.sleep(0.2)

		print(f"Scanning for BLE ID containing '{_OUR_PERIPH_str}' {'.'*scan_count:20}  \r", end="")
		scan_count += 1
		if scan_count > 20:
			scan_count = 0
	print()

	# "periph" will be set by the on_scan_found() callback.
	if periph == None:
		print(f"Did not find the '{_OUR_PERIPH_str}' peripheral. Trying again...")
		continue

	print(f"\n   -- Found {periph.identifier()}!!  Trying to connect now...")
	periph.connect()

	print("   -- Connected!")
	services = periph.services()

	# # Jump to Python interactive prompt, so we can tinker with services.
	# vars = globals()
	# vars.update(locals())
                                              
	# readline.set_completer(rlcompleter.Completer(vars).complete)
	# readline.parse_and_bind("tab: complete")
	# #code.InteractiveConsole(vars).interact()
	# code.interact(local=vars)

	# List the Service and Characteristic UUID's.
	for svc in services:
		print(f'Service: {svc.uuid()}')
		for chrst in svc.characteristics():
			print(f'   {chrst.uuid()}  -- {chrst.capabilities()}')

	# This is the callback function for whenever the peripheral sends us some bytes.
	def GetNotification(data):
		print(f"Notification: {data.decode()}")

		# Note that periph.mtu() only shoes the "software" GATT MTU, not the
		# hardware PHY max packet length. So it is not useful for determining
		# whether we are actually using DLE long radio packets over the air.
		
		#print(f"(MTU {periph.mtu()}) Notification: {data.decode()}")
		#print(f"Notification: {data}")

	# Enable the above callback function for each "Notification" from the peripheral.
	periph.notify(SERVICE, CHAR_periph_Notify, GetNotification)

	countup = 0

	# =========================================================
	# This inner loop continues as long as we are connected.
	#
	print("\nPress ESC or 'q' to stop the loop, 'w' to write string, or send param commands:\n")
	print("'x' *start")
	print("'y' *stop")
	print("'I' *setp enable_imu 1")
	print("'i' *setp enable_imu 0")
	print("'H' *setp enable_H_imu 1")
	print("'h' *setp enable_H_imu 0")
	print("'M' *setp enable_mic 1")
	print("'m' *setp enable_mic 0")
	print("'C' *setp enable_cam 1")
	print("'c' *setp enable_cam 0")
	print("'0' *setp imu_fs 1000")
	print("'1' *setp imu_fs 500")
	print("'2' *setp imu_fs 200")
	print("'3' *setp imu_fs 100")
	print("'4' *setp mic_fs 4000000")
	print("'5' *setp mic_fs 2000000")
	print("'6' *setp mic_fs 1000000")
	print("'7' *setp mic_fs 500000")
	print("'8' *setp cam_fs 20")
	print("'9' *setp cam_fs 10")

	while periph.is_connected():
		time.sleep(0.2)
		if not msvcrt.kbhit():
			continue

		# Q or ESC to quit program.
		ch = msvcrt.getch().decode()  # MUST decode b-string to get "normal" string.

		print(f"  -- Got key press: {ch} --")

		if (ch.lower() == 'q') or (ch == chr(27)):
			KeepGoing = False
			break

		# Re-connect.
		if ch.lower() == 'r':
			print("Disconnecting and reconnecting...")
			break

		# elif ch == 'w':
		# 	print(" -- Writing a string to the peripheral...")
		# 	# write expects a Byte array, so we must "encode()" the string.
		# 	periph.write_command(SERVICE, CHAR_periph_RX, f"String CMD from Central! {countup}".encode())
		# 	#periph.write_request(SERVICE, CHAR_periph_RX, f"String from Central! {countup}".encode())
		# 	countup += 1

		# elif ch == 'W':
		# 	print(" -- Writing a string to the peripheral...")
		# 	# write expects a Byte array, so we must "encode()" the string.
		# 	#periph.write_command(SERVICE, CHAR_periph_RX, f"String from Central! {countup}".encode())
		# 	periph.write_request(SERVICE, CHAR_periph_RX, f"String REQ from Central! {countup}".encode())
		# 	countup += 1

		# Some demo commands to set "parameters" in the peripheral.

		# elif ch == '>':
		# 	param2 += 20
		# 	periph.write_command(SERVICE, CHAR_periph_RX, f"*setp2 {param2}".encode())

		# elif ch == '<':
		# 	param2 -= 20
		# 	periph.write_command(SERVICE, CHAR_periph_RX, f"*setp2 {param2}".encode())

		# elif ch == '.':
		# 	param1 += 1
		# 	periph.write_command(SERVICE, CHAR_periph_RX, f"*setp1 {param1}".encode())

		# elif ch == ',':
		# 	param1 -= 1
		# 	periph.write_command(SERVICE, CHAR_periph_RX, f"*setp1 {param1}".encode())

		# elif ch == 'b':
		# 	# Change both parameters at once.
		# 	periph.write_command(SERVICE, CHAR_periph_RX, f"*setboth 987 654 12.34".encode())

		#### enable/disable IMU
		elif ch == 'I':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_imu 1".encode())
		elif ch == 'i':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_imu 0".encode())
		
		#### enable/disable head IMU
		elif ch == 'H':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_H_imu 1".encode())
		elif ch == 'h':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_H_imu 0".encode())

		#### enable/disable microphone
		elif ch == 'M':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_mic 1".encode())
		elif ch == 'm':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_mic 0".encode())

		#### enable/disable camera
		elif ch == 'C':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_cam 1".encode())
		elif ch == 'c':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp enable_cam 0".encode())

		#### trial start/stop
		elif ch == 'x':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*start".encode())
		elif ch == 'y':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*stop".encode())

		#### change imu rate
		elif ch == '0':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp imu_fs 1000".encode())
		elif ch == '1':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp imu_fs 500".encode())
		elif ch == '2':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp imu_fs 200".encode())
		elif ch == '3':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp imu_fs 100".encode())

		#### change mic rate
		elif ch == '4':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp mic_fs 4000000".encode())
		elif ch == '5':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp mic_fs 2000000".encode())
		elif ch == '6':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp mic_fs 1000000".encode())
		elif ch == '7':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp mic_fs 500000".encode())

		#### change cam rate
		elif ch == '8':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp cam_fs 20".encode())
		elif ch == '9':
			periph.write_command(SERVICE, CHAR_periph_RX, f"*setp cam_fs 10".encode())

		

	# Flush out any extra key presses.
	# while msvcrt.kbhit():
	# 	msvcrt.getch()

	print("\n - disconnecting...")
	periph.disconnect()
	print("Disconnected. Done!\n")
