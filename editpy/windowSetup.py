#This module sets the window and scroll area for the GUI
#Window Dimensions
import initSelf
self.setGeometry(300, 300, 600, 600)
self.setWindowTitle('varDB GUI')
#Create middle division
hbox = QtWidgets.QHBoxLayout(self)
self.left = QtWidgets.QFrame(self)
self.left.setFrameShape(QtWidgets.QFrame.StyledPanel)
self.right = QtWidgets.QFrame(self)
self.right.setFrameShape(QtWidgets.QFrame.StyledPanel)
splitter1 = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
splitter1.addWidget(self.left)
splitter1.addWidget(self.right)
splitter1.setSizes([150,300])
hbox.addWidget(splitter1)
self.setLayout(hbox)
QtWidgets.QApplication.setStyle(QtWidgets.QStyleFactory.create('Cleanlooks'))
#Create scroll area
self.left.scrollArea=QtWidgets.QScrollArea(self.left)
self.left.scrollArea.setWidgetResizable(True)
self.left.scrollAreaWidgetContents=QtWidgets.QWidget(self.left.scrollArea)
self.left.scrollArea.setWidget(self.left.scrollAreaWidgetContents)
self.left.verticalLayout=QtWidgets.QVBoxLayout(self.left)
self.left.verticalLayout.addWidget(self.left.scrollArea)
self.left.verticalLayoutScroll=QtWidgets.QVBoxLayout(self.left.scrollAreaWidgetContents)

