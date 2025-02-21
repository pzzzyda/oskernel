#include "fs/fcntl.h"
#include "fs/stat.h"
#include "ulib.h"

char buf[1024];

int cp(char *src, char *dst)
{
	int sfd, dfd;
	ssize_t n;
	struct stat st;

	sfd = open(src, O_RDONLY);
	if (sfd < 0) {
		dprintf(2, "cannot open %s\n", src);
		return -1;
	}

	if (fstat(sfd, &st) < 0) {
		dprintf(2, "cannot stat %s\n", src);
		close(sfd);
		return -1;
	}

	if (st.type == FT_DIR) {
		dprintf(2, "%s is directory\n");
		close(sfd);
		return -1;
	}

	dfd = open(dst, O_CREAT | O_TRUNC | O_WRONLY);
	if (dfd < 0) {
		close(sfd);
		dprintf(2, "cannot open %s\n", dst);
		return -1;
	}

	while ((n = read(sfd, buf, 1024)) > 0) {
		if (write(dfd, buf, n) != n) {
			close(sfd);
			close(dfd);
			unlink(dst);
			return -1;
		}
	}

	close(sfd);
	close(dfd);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		dprintf(2, "usage: %s [source] [dest]\n", argv[0]);
		exit(1);
	}
	if (cp(argv[1], argv[2]) < 0) {
		dprintf(2, "cp %s to %s failed\n", argv[1], argv[2]);
		exit(1);
	}
	return 0;
}
