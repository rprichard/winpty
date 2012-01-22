all :
	cd agent && $(MAKE)
	cd libpconsole && $(MAKE)

clean :
	cd agent && $(MAKE) clean
	cd libpconsole && $(MAKE) clean
