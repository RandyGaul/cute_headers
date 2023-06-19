CC     = clang
CINC   = -I/opt/homebrew/include -L/opt/homebrew/lib
CLINK  = -ls2n
CFLAGS = $(CINC) $(CLINK)

hello : main.c ../../../cute_tls.h
	$(CC) main.c -DCUTE_TLS_S2N -o $@ $(CFLAGS)
