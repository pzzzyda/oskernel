#include "ulib.h"

int main(int argc, char *argv[])
{
	int i;

	if (argc < 2) {
		dprintf(2, "usage: %s [file]\n", argv[0]);
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		if (unlink(argv[i]) < 0) {
			dprintf(2, "failed to delete %s\n", argv[i]);
			break;
		}
	}

	return 0;
}
