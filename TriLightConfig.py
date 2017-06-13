#Trilight configuration utility
#By Silas Alrøe
#REQUIRES pySerial! (And a linux pc...)
import serial           #Used for communication with arduino
import datetime         #Gets current time from system so user does not have to input it
import subprocess as sp #Used for clearing terminal screen for neatness

tmp = sp.call('clear',shell=True)   #Clear terminal

ser = serial.Serial('/dev/ttyUSB0', 9600)   #Initialize serial connection to lamp
print( "  _______   _ _      _       _     _   ")   #Print ascii graphic
print( " |__   __| (_) |    (_)     | |   | |  ")   #Just because it looks nice
print( "    | |_ __ _| |     _  __ _| |__ | |_ ")
print( "    | | '__| | |    | |/ _\`| '_ \| __|")
print( "    | | |  | | |____| | (_| | | | | |_ ")
print( "    |_|_|  |_|______|_|\__, |_| |_|\__|")
print( "                        __/ |          ")
print( "                       |___/           ")
print("")

print ("Welcome to the TriLight configuration utility") #Print welcome
print ("")
print("Please input the time in the evening you want to start relaxing")    #Request sundown time in hh/mm
sDH = raw_input("Hour: ")   #Input_raw displays text and returns following user-inputted line from terminal
sDM = raw_input("Minute: ")
print ("")

print("Please input the time you get up")   #Request sunrise time in hh/mm
sRH = raw_input("Hour: ")
sRM = raw_input("Minute: ")
print ("")

print("Please input the time to transition the light")  #Request transition time in hh/mm
iH = raw_input("Hour: ")
iM = raw_input("Minute: ")
print ("")

print("Please input the day and night temperature (100 - 10 000)")  #Request min and max temperature to cycle between
tH = raw_input("Day: ")
tL = raw_input("Night: ")
print ("")

print("Thank you, setting time and configuration")  #Print thank you

now = datetime.datetime.now() #Get current date/time

sendString = str(now.hour)+"x"+str(now.minute)+"x"+sDH+"x"+sDM+"x"+sRH+"x"+sRM+"x"+iH+"x"+iM+"x"+tL+"x"+tH  #Composite string fo sending
print("Sending: "+sendString)   #Print pending action to user
ser.write(sendString+"\n")      #Send string to arduino

print("Response: "+str(ser.readline())) #Print response from arduino to user to check success
