from ctypes import *
from gtts import gTTS
import ctypes
import pyfirmata
import time
import os
import speech_recognition as sr

r = sr.Recognizer()
so_file = "/home/simon/Documents/automatic_chessboard/ChessAlgorithm.so"
#AZURE_SPEECH_KEY = "f84602d441ba4ce6b6ff2aa108185ba9"

chess_algorithm = CDLL(so_file)
chess_algorithm.run_chess_algorithm.argtypes = c_char_p,
chess_algorithm.get_int_command_value.restype = c_int32
chess_algorithm.get_float_command_value_a.restype = c_float
chess_algorithm.get_float_command_value_b.restype = c_float
chess_algorithm.get_tts.restype = c_char_p
chess_algorithm.is_running.restype = c_bool

def tts(message):
    tts = gTTS(text=message, lang='en')
    tts.save('tts.mp3')
    os.system('mpg123 tts.mp3')

def from_mic():
    with sr.Microphone() as source:
        print("Say something!")
        audio = r.listen(source)
        try:
            #message = r.recognize_azure(audio, key=AZURE_SPEECH_KEY)
            message = r.recognize_google(audio)
            print(message)
            return message
        except sr.UnknownValueError:
            print("Speech not recognized.")
        except sr.RequestError as e:
            print("Could not request results from service; {0}".format(e))
        return ""
    #speech_config = speechsdk.SpeechConfig(subscription="f84602d441ba4ce6b6ff2aa108185ba9", region="eastus")
    #speech_recognizer = speechsdk.SpeechRecognizer(speech_config=speech_config)
    #result = speech_recognizer.recognize_once_async().get()
    #print("You said: ", result.text)
    #return result.text

def prompt_input(currentTurn):
    if(chess_algorithm.is_running() == False): #no more input, game is done
        print("Game over!")
        quit()
    print("It's white's turn:" if chess_algorithm.get_turn() == chess_algorithm.get_white() else "It's black's turn:")
    if (chess_algorithm.get_turn() == chess_algorithm.get_white() and currentTurn != chess_algorithm.get_white()):
        tts("It's white's turn!")
        currentTurn = chess_algorithm.get_turn()
    elif (chess_algorithm.get_turn() != chess_algorithm.get_white() and currentTurn == chess_algorithm.get_white()):
        tts("It's black's turn!")
        currentTurn = chess_algorithm.get_turn()
    print("You may speak now.")

    #command = input()
    command = from_mic()
    b_command = command.encode()

    buf = create_string_buffer(1024)
    buf.value = b_command
    chess_algorithm.run_chess_algorithm(buf)
    return currentTurn

if __name__ == '__main__':
    tts("Welcome to automatic chess!")

    board = pyfirmata.Arduino('/dev/ttyACM0')
    print("Communication successfully started")
    board.digital[8].write(0)

    #hardware pins (don't change!!)	
    motorXDir = 5
    motorYDir = 6
    motorZDir = 7

    motorXStep = 2
    motorYStep = 3
    motorZStep = 4	

    #linear motion variables
    #tile A1 is the vertex of the sides with the motors
    unitStep = 213 #moves motor exactly 1 tile, with sleep time between turning on/off
    unitTurn = 30 #turns the magnet motor on/off

    targetFilePos = 0 # (0-2130)
    targetRankPos = 0
    targetMagnetPos = 0 # desired angle, starts at 0
    curFilePos = 0
    curRankPos = 0
    curMagnetPos = 0

    currentTurn = -1
    chess_algorithm.init_board()
    chess_algorithm.print_board()

    while True:
        tts_message = chess_algorithm.get_tts().decode()
        if(tts_message != ""):
            tts(tts_message)
            #time.sleep(0.5)
            continue

        board.digital[motorXDir].write(1 if curMagnetPos < targetMagnetPos else 0)
        board.digital[motorYDir].write(1 if curRankPos < targetRankPos else 0)
        board.digital[motorZDir].write(1 if curFilePos < targetFilePos else 0)

        if(curFilePos == targetFilePos and curRankPos == targetRankPos and curMagnetPos == targetMagnetPos): #check for next command or prompt input
            if(chess_algorithm.has_commands()): #queue up next command
                command_type = chess_algorithm.get_command_type()
                if(command_type == 0): #toggle magnet
                    targetMagnetPos = chess_algorithm.get_int_command_value() * unitTurn
                    #print("Setting magnet to: ", targetMagnetPos)
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
                time.sleep(0.2)
            else: #reset board and prompt input
                chess_algorithm.motor_reset()
                if(chess_algorithm.has_commands() == False): #no motor reset needed
                    currentTurn = prompt_input(currentTurn)
        else: #configure hardware to reach target states
            if(targetMagnetPos != curMagnetPos): #move magnet motor
                board.digital[motorXStep].write(1)
                curMagnetPos += (1 if targetMagnetPos > curMagnetPos else -1)
                time.sleep(0.02)

                board.digital[motorXStep].write(0)
            else: #move motors
                #print(curFilePos, " -> ", targetFilePos, " & ", curRankPos, " -> ", targetRankPos)
                if(curFilePos != targetFilePos): #move motors that control file
                    board.digital[motorZStep].write(1)
                    curFilePos += (1 if targetFilePos > curFilePos else -1)
                if(curRankPos != targetRankPos):
                    board.digital[motorYStep].write(1)
                    curRankPos += (1 if targetRankPos > curRankPos else -1)
                time.sleep(0.002)
	
                board.digital[motorYStep].write(0)
                board.digital[motorZStep].write(0)
                #time.sleep(0.001)
