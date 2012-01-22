all :
	cd Agent && $(MAKE)
	cd libpconsole && $(MAKE)

clean :
	cd Agent && $(MAKE) clean
	cd libpconsole && $(MAKE) clean
