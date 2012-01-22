ifndef QT_DIR
    $(error The QT_DIR variable must be defined to the QtSDK mingw directory containing bin, include, and lib)
endif

QT_INCLUDE=${QT_DIR}/include
QT_LIB=${QT_DIR}/lib
MOC=${QT_DIR}/bin/moc

.PRECIOUS : moc_%.cc
moc_%.cc : %.h
	@echo Making $@ from $<
	@$(MOC) -o $@ $<

CXXFLAGS += \
	-DUNICODE \
	-D_UNICODE \
	-I$(QT_INCLUDE) \
	-I$(QT_INCLUDE)/Qt \
	-I$(QT_INCLUDE)/QtCore

LDFLAGS += \
	-municode \
	-L$(QT_LIB) \
	-lQtCore4
