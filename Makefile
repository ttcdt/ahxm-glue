PREFIX=/usr/local/bin

ahxm-glue: ahxm-glue.c
	$(CC) $< -lm -o $@

install:
	install ahxm-glue $(PREFIX)/ahxm-glue

uninstall:
	rm -f $(PREFIX)/ahxm-glue

dist: clean
	rm -f ahxm-glue.tar.gz && cd .. && tar czvf ahxm-glue/ahxm-glue.tar.gz ahxm-glue/*

clean:
	rm -f ahxm-glue *.tar.gz *.asc
