/*
 * przyk³ad prostego programu ³±cz±cego siê z serwerem i wysy³aj±cego
 * jedn± wiadomo¶æ.
 */

#include <stdio.h>
#include <stdlib.h>
#include "libgg.h"

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "u¿ycie: %s <email> <has³o>\n", argv[0]);
		return 1;
	}

	printf("%u\n", gg_http_hash(argv[1], argv[2]));

	return 0;
}

