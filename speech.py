
import speech_recognition as sr

talk = sr.Recognizer()
with sr.Microphone() as source:
    print("Make your move:")
    audio = talk.listen(source)
    try:
        text = talk.recognize_google(audio)
        print("Your move: {}".format(text))
    except:
        print("Couldn't recognize move.")
