// SPDX-License-Identifier: GPL-2.0

/* NOTE: we really do want to use the kernel headers here */
#define __EXPORTED_HEADERS__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

struct security_class_mapping {
	const char *name;
	const char *perms[sizeof(unsigned) * 8 + 1];
};

#include "classmap.h"
#include "initial_sid_to_string.h"

const char *progname;

static void usage(void)
{
	printf("usage: %s flask.h av_permissions.h\n", progname);
	exit(1);
}

static char *stoupperx(const char *s)
{
	char *s2 = strdup(s);
	char *p;

	if (!s2) {
		fprintf(stderr, "%s:  out of memory\n", progname);
		exit(3);
	}

	for (p = s2; *p; p++)
		*p = toupper(*p);
	return s2;
}

int main(int argc, char *argv[])
{
	int i, j;
	int isids_len;
	FILE *fout;

	progname = argv[0];

	if (argc < 3)
		usage();

	fout = fopen(argv[1], "w");
	if (!fout) {
		fprintf(stderr, "Could not open %s for writing:  %s\n",
			argv[1], strerror(errno));
		exit(2);
	}

	fprintf(fout, "/* This file is automatically generated.  Do not edit. */\n");
	fprintf(fout, "#ifndef _SELINUX_FLASK_H_\n#define _SELINUX_FLASK_H_\n\n");

	for (i = 0; secclass_map[i].name; i++) {
		char *name = stoupperx(secclass_map[i].name);

		fprintf(fout, "#define SECCLASS_%-39s %2d\n", name, i+1);
		free(name);
	}

	fprintf(fout, "\n");

	isids_len = sizeof(initial_sid_to_string) / sizeof(char *);
	for (i = 1; i < isids_len; i++) {
		const char *s = initial_sid_to_string[i];
		if (s) {
			char *sidname = stoupperx(s);

			fprintf(fout, "#define SECINITSID_%-39s %2d\n", sidname, i);
			free(sidname);
		}
	}
	fprintf(fout, "\n#define SECINITSID_NUM %d\n", i-1);
	fprintf(fout, "\nstatic inline bool security_is_socket_class(u16 kern_tclass)\n");
	fprintf(fout, "{\n");
	fprintf(fout, "\tbool sock = false;\n\n");
	fprintf(fout, "\tswitch (kern_tclass) {\n");
	for (i = 0; secclass_map[i].name; i++) {
		static char s[] = "SOCKET";
		int len, l;
		char *name = stoupperx(secclass_map[i].name);

		len = strlen(name);
		l = sizeof(s) - 1;
		if (len >= l && memcmp(name + len - l, s, l) == 0)
			fprintf(fout, "\tcase SECCLASS_%s:\n", name);
		free(name);
	}
	fprintf(fout, "\t\tsock = true;\n");
	fprintf(fout, "\t\tbreak;\n");
	fprintf(fout, "\tdefault:\n");
	fprintf(fout, "\t\tbreak;\n");
	fprintf(fout, "\t}\n\n");
	fprintf(fout, "\treturn sock;\n");
	fprintf(fout, "}\n");

	fprintf(fout, "\n#endif\n");

	if (fclose(fout) != 0) {
		fprintf(stderr, "Could not successfully close %s:  %s\n",
			argv[1], strerror(errno));
		exit(4);
	}

	fout = fopen(argv[2], "w");
	if (!fout) {
		fprintf(stderr, "Could not open %s for writing:  %s\n",
			argv[2], strerror(errno));
		exit(5);
	}

	fprintf(fout, "/* This file is automatically generated.  Do not edit. */\n");
	fprintf(fout, "#ifndef _SELINUX_AV_PERMISSIONS_H_\n#define _SELINUX_AV_PERMISSIONS_H_\n\n");

	for (i = 0; secclass_map[i].name; i++) {
		const struct security_class_mapping *map = &secclass_map[i];
		int len;
		char *name = stoupperx(map->name);

		len = strlen(name);
		for (j = 0; map->perms[j]; j++) {
			char *permname;

			if (j >= 32) {
				fprintf(stderr, "Too many permissions to fit into an access vector at (%s, %s).\n",
					map->name, map->perms[j]);
				exit(5);
			}
			permname = stoupperx(map->perms[j]);
			fprintf(fout, "#define %s__%-*s 0x%08xU\n", name,
				39-len, permname, 1U<<j);
			free(permname);
		}
		free(name);
	}

	fprintf(fout, "\n#endif\n");

	if (fclose(fout) != 0) {
		fprintf(stderr, "Could not successfully close %s:  %s\n",
			argv[2], strerror(errno));
		exit(6);
	}

	exit(0);
}
