#include "ulib.h"

int main(int argc, char *argv[])
{
	int i;

	if (argc < 2) {
		dprintf(2, "usage: %s [pid]\n", argv[0]);
		exit(1);
	}

	for (i = 1; i < argc; i++)
		kill(atoi(argv[i]));

	return 0;
}
