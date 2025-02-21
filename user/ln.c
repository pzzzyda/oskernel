#include "ulib.h"

int main(int argc, char *argv[])
{
	if (argc != 3) {
		dprintf(2, "usage: %s [from] [to]\n", argv[0]);
		exit(1);
	}

	if (link(argv[1], argv[2]) < 0) {
		dprintf(2, "link %s %s failed\n", argv[1], argv[2]);
		exit(1);
	}

	return 0;
}
