all :
	cd agent && $(MAKE)
	cd libpconsole && $(MAKE)
	cd unix-adapter && $(MAKE)

clean :
	cd agent && $(MAKE) clean
	cd libpconsole && $(MAKE) clean
	cd unix-adapter && $(MAKE) clean
