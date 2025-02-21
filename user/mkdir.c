#include "ulib.h"

int main(int argc, char *argv[])
{
	int i;

	if (argc < 2) {
		dprintf(2, "usage: %s [directory]\n", argv[0]);
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		if (mkdir(argv[i]) < 0) {
			dprintf(2, "failed to create %s\n", argv[i]);
			break;
		}
	}

	return 0;
}
