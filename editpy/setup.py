#Julian QUick
#Sets up GUI Window, scroll area, and data lables
import sys

fileLocation='NULL'
def fileName():
      return fileLocation

def setup(hbox,self,file):

      #Establish filename
      global fileLocation
      if len(sys.argv)==1:
        print("please enter filename in command line")
        quit()
      fileLocation=str(sys.argv[1])

      self.saveChanges=False

      #some definitions
      from radioClickEvent import lookingAt
      from PyQt5 import QtWidgets, QtCore
      self.booleanList=['reference','is_analog']
      self.catelogList=[['standard_name','standardNames'],['category','Categories']]

      #Create division between right up and down layouts
      splitter1=QtWidgets.QSplitter(QtCore.Qt.Vertical)
      splitter1.addWidget(self.upright)
      splitter1.addWidget(self.downright)
      splitter1.setStretchFactor(1,0)
      splitter1.setSizes([560,40])
      hbox.addWidget(splitter1)

      #Create middle division
      splitter2 = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
      splitter2.addWidget(self.left)
      splitter2.addWidget(splitter1)
      splitter2.setStretchFactor(1,1)
      splitter2.setSizes([180,420])
      hbox.addWidget(splitter2)
      self.left.setFrameShape(QtWidgets.QFrame.StyledPanel)
      self.downright.setFrameShape(QtWidgets.QFrame.StyledPanel)
      self.upright.setFrameShape(QtWidgets.QFrame.StyledPanel)
      QtWidgets.QApplication.setStyle(QtWidgets.QStyleFactory.create('Cleanlooks'))

#===========================================
#Create scroll areas

#------------------------------------------------
#left area
  
      #Create scroll area
#      self.left.scrollArea=QtWidgets.QScrollArea(self.left)
#      self.left.scrollArea.setWidgetResizable(True)
#      self.left.scrollAreaWidgetContents=QtWidgets.QListWidget(self.left.scrollArea)

      self.left.scrollAreaWidgetContents=QtWidgets.QListWidget(self.left)

      self.left.scrollAreaWidgetContents.itemSelectionChanged.connect(lambda:lookingAt(self))
#      self.left.scrollArea.setWidget(self.left.scrollAreaWidgetContents)
      self.left.verticalLayout=QtWidgets.QVBoxLayout(self.left)
      self.left.verticalLayoutScroll=QtWidgets.QVBoxLayout(self.left.scrollAreaWidgetContents)
      
      #Create search bar
      searchLabel = QtWidgets.QLabel(" Search", self)
      self.searchText = QtWidgets.QLineEdit()
      searchLabel.setBuddy(self.searchText)

      #Populate left side
      self.left.verticalLayout.addWidget(searchLabel)
      self.left.verticalLayout.addWidget(self.searchText)
      self.left.verticalLayout.addWidget(self.left.scrollAreaWidgetContents)

#------------------------------------------------
#right area

      self.upright.scrollArea=QtWidgets.QScrollArea(self.upright)
      self.upright.scrollArea.setWidgetResizable(True)
      self.upright.scrollAreaWidgetContents=QtWidgets.QWidget(self.upright.scrollArea)
      self.upright.scrollArea.setWidget(self.upright.scrollAreaWidgetContents)
      self.upright.verticalLayout=QtWidgets.QVBoxLayout(self.upright)
      self.upright.verticalLayout.addWidget(self.upright.scrollArea)
      self.upright.verticalLayoutScroll=QtWidgets.QGridLayout(self.upright.scrollAreaWidgetContents)
      #self.upright.verticalLayoutScroll=QtWidgets.QVBoxLayout(self.upright.scrollAreaWidgetContents)
#===================================================================

      return self
