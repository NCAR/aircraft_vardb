#Julian Quick
#This function comfirms the user desires to remove a variable, and calls the remove function
import getInfo
import remove
from setup import fileName
from PyQt5 import QtWidgets, QtCore
def delete(signame,self,num):
   entries=getInfo.getinfo(fileName())
   quit_messge='Are you sure you want to delete '+signame
   reply=QtWidgets.QMessageBox.question(self, 'Warning: altering varDB', quit_messge, QtWidgets.QMessageBox.Yes, QtGui.QMessageBox.No)
   if reply == QtWidgets.QMessageBox.Yes:
      from addSignal import addsignal
      from radioClickEvent import lookingAt
      from radioClickEvent import clearRightInfoHub
      clearRightInfoHub()
      #lookingAt(-1,self)
      addsignal(signame,self,num,{'action':'delete'})
