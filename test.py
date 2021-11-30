from ctypes import *
import azure.cognitiveservices.speech as speechsdk
import ctypes
import pyfirmata
import time
import pyttsx3

engine = pyttsx3.init() #initializes the speaker listening 

so_file = "ChessAlgorithm.so"

chess_algorithm = CDLL(so_file)
chess_algorithm.run_chess_algorithm.argtypes = c_char_p,
chess_algorithm.get_int_command_value.restype = c_int32
chess_algorithm.get_float_command_value_a.restype = c_float
chess_algorithm.get_float_command_value_b.restype = c_float
chess_algorithm.get_tts.restype = c_char_p
chess_algorithm.is_running.restype = c_bool

def from_mic():
	speech_config = speechsdk.SpeechConfig(subscription="f84602d441ba4ce6b6ff2aa108185ba9", region="eastus")
	speech_recognizer = speechsdk.SpeechRecognizer(speech_config=speech_config)
	result = speech_recognizer.recognize_once_async().get()
	print("You said: ", result.text)
	return result.text

def prompt_input(currentTurn):
	if(chess_algorithm.is_running() == False): #no more input, game is done
		print("Game over!")
		quit()
	print("It's white's turn:" if chess_algorithm.get_turn() == chess_algorithm.get_white() else "It's black's turn:")
	if (chess_algorithm.get_turn() == chess_algorithm.get_white() and currentTurn != chess_algorithm.get_white()):
		engine.say("It's white's turn") #speaks it out loud
		currentTurn = chess_algorithm.get_turn()
		engine.runAndWait() #runs it
		time.sleep(0.5) # waits so whatever is spoken here aloud isn't picked up by the speech-to-text mic
	elif (chess_algorithm.get_turn() != chess_algorithm.get_white() and currentTurn == chess_algorithm.get_white()):
		engine.say("It's black's turn")
		currentTurn = chess_algorithm.get_turn()
		engine.runAndWait() # waits so whatever is spoken here aloud isn't picked up by the speech-to-text mic
		time.sleep(0.5)
	print("You may speak now.")

 	#command = input()
	command = from_mic()
	b_command = command.encode()

	buf = create_string_buffer(1024)
	buf.value = b_command
	chess_algorithm.run_chess_algorithm(buf)
	return currentTurn

if __name__ == '__main__':
	board = pyfirmata.Arduino('/dev/cu.usbmodem141301')
	print("Communication successfully started")
	board.digital[8].write(0)

	#hardware pins (don't change!!)	
	motorXDir = 5
	motorYDir = 6
	motorZDir = 7

	motorXStep = 2
	motorYStep = 3
	motorZStep = 4	
	
	electromagnet = 13

	#linear motion variables
	#tile A1 is the vertex of the sides with the motors
	unitStep = 222 #moves motor exactly 1 tile, with sleep time of 0.001 between turning on/off
	targetFilePos = 0 # (0-2220)
	targetRankPos = 0
	targetMagnetState = 0
	curFilePos = 0
	curRankPos = 0
	curMagnetState = 0

	currentTurn = -1
	chess_algorithm.init_board()
	chess_algorithm.print_board()

	while True:
		tts_message = chess_algorithm.get_tts().decode()
		if(tts_message != ""):
			engine.say(tts_message)
			engine.runAndWait()
			#time.sleep(0.5)
			continue

		board.digital[motorXDir].write(0 if curFilePos < targetFilePos else 1)
		board.digital[motorYDir].write(1 if curRankPos < targetRankPos else 0)
		board.digital[motorZDir].write(1 if curFilePos < targetFilePos else 0)

		if(curFilePos == targetFilePos and curRankPos == targetRankPos and curMagnetState == targetMagnetState): #check for next command or prompt input
			if(chess_algorithm.has_commands()): #queue up next command
				command_type = chess_algorithm.get_command_type()
				if(command_type == 0): #toggle magnet
					targetMagnetState = chess_algorithm.get_int_command_value()
					#print("Setting magnet to: ", targetMagnetState)
				elif(command_type == 1): #change file
					targetFilePos = curFilePos + chess_algorithm.get_float_command_value_b() * unitStep
					#print("Target file position: ", targetFilePos)
				elif(command_type == 2): #change rank
					targetRankPos = curRankPos + chess_algorithm.get_float_command_value_b() * unitStep
					#print("Target rank position: ", targetRankPos)
				elif(command_type == 3): #change rank AND file
					#print("HELLOOO ", chess_algorithm.get_float_command_value_a())
					targetFilePos = round(curFilePos + chess_algorithm.get_float_command_value_a() * unitStep, 0)
					targetRankPos = round(curRankPos + chess_algorithm.get_float_command_value_b() * unitStep, 0)
					#print("Target position: (", targetFilePos, ", ", targetRankPos, ")")
				#time.sleep(0.2)
			else: #prompt input
				currentTurn = prompt_input(currentTurn)
		else: #configure hardware to reach target states
			if(targetMagnetState != curMagnetState): #toggle magnet
				curMagnetState = targetMagnetState
				board.digital[electromagnet].write(curMagnetState)
				time.sleep(2)
			else: #move motors
				#print(curFilePos, " -> ", targetFilePos, " & ", curRankPos, " -> ", targetRankPos)
				if(curFilePos != targetFilePos): #move motors that control file
					board.digital[motorXStep].write(1)
					board.digital[motorZStep].write(1)
					curFilePos += (1 if targetFilePos > curFilePos else -1)
				if(curRankPos != targetRankPos):
					board.digital[motorYStep].write(1)
					curRankPos += (1 if targetRankPos > curRankPos else -1)
				time.sleep(0.0015 if curMagnetState == 1 else 0.0001)
	
				board.digital[motorXStep].write(0)
				board.digital[motorYStep].write(0)
				board.digital[motorZStep].write(0)
				#time.sleep(0.001)
