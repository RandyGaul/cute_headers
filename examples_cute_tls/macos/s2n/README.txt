The makefile likely needs to be editted. Make sure you have CINC setup for include folders
for both the s2n library and it's include headers. There are a variety of ways to build and
install s2n, though I'd recommend to just use brew:

	brew install s2n

Just make sure your brew install folders (for the lib + headers) are available to the compiler.

You also need to define CUTE_TLS_S2N before including cute_tls.h to force s2n usage over
Network.framework.
