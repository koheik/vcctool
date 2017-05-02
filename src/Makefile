all: vcc

vcc: vcc.c
	$(CC) -I/usr/local/Cellar/libftdi/1.3/include/libftdi1 \
	-L/usr/local/Cellar/libftdi/1.3/lib -lftdi1.2 \
	 $< -o $@

clean:
	$(RM) vcc
