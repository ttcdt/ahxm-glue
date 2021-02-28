PREFIX=/usr/local/bin

ahxm-glue: flac-decode.o ahxm-glue.o
	@echo linking
	@$(CC) -g *.o -lm `cat .ldflags` -o $@

flac-decode.o: flac-decode.c
	@echo compiling $<
	@echo "-lFLAC" > .ldflags
	@$(CC) -g -c $< || \
	(echo "FLAC not available... disabling" ; echo "" > .ldflags ; $(CC) -DDISABLE_FLAC -c $<)

ahxm-glue.o: ahxm-glue.c
	@echo compiling $<
	@$(CC) -g -c $<

install:
	install ahxm-glue $(PREFIX)/ahxm-glue

uninstall:
	rm -f $(PREFIX)/ahxm-glue

dist: clean
	rm -f ahxm-glue.tar.gz && cd .. && tar czvf ahxm-glue/ahxm-glue.tar.gz ahxm-glue/*

clean:
	rm -f ahxm-glue *.tar.gz *.asc *.o .ldflags
