#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "anagram.h"

float delta(struct timeval *b, struct timeval *a);
int cb(void *argument, int count, const char *anagram);

int main(int argc, char *argv[])
{

	FILE *fp;
	struct timeval ti, tf;
	float dt;
	anagram_ref anagram;
	int i, c, seq;
	char *buf;

	if (argc < 2) {
		printf("Usage: %s ANAGRAM_STRING [FILTER]\n\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* allocate buffer */
	buf = malloc(1024);
	if (buf == NULL) {
		printf("Error allocating memory #%04d\n", errno);
		exit(EXIT_FAILURE);
	}

	/* build anagram path */
	sprintf(buf, "%s.anagram", argv[1]);

	seq = 1;

	/* open or create anagram */
	printf("%d. Opening anagram file \"%s\"...\n", seq++, buf);
	anagram = anagram_open(buf);
	if (anagram != NULL) {
		printf("\tAnagram file with source string \"%s\" successfully opened.\n\n", anagram_source_string(anagram));
		printf("%d. Performing integrity test...\n", seq++);
		gettimeofday(&ti, NULL);
		if (!anagram_test(anagram, NULL, cb)) {
			printf("Error testing anagram #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		gettimeofday(&tf, NULL);
		dt = delta(&tf, &ti);
		printf("\tIntegrity test successfully performed on %d permutations in %0.4f seconds.\n\n", anagram_permutation_count(anagram), dt);
	}
	else if (anagram == NULL && errno == ENOENT) {

		puts("\tAnagram file not found!\n");

		/* initialize file */
		printf("%d. Initializing anagram file \"%s\"...\n", seq++, buf);
		anagram = anagram_create(buf, argv[1]);
		if (anagram == NULL) {
			printf("Error initializing anagram file #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		printf("\tAnagram file successfully initialized with string \"%s\".\n\n", argv[1]);

		/* list generation */
		printf("%d. Generating permutations...\n", seq++);
		gettimeofday(&ti, NULL);
		if (!anagram_generate(anagram, NULL, cb)) {
			printf("Error generating permutations #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		gettimeofday(&tf, NULL);
		dt = delta(&tf, &ti);
		printf("\t%d permutations successfully generated in %0.4f seconds.\n\n", anagram_permutation_count(anagram), dt);

		/* integrity test */
		printf("%d. Performing integrity test...\n", seq++);
		gettimeofday(&ti, NULL);
		if (!anagram_test(anagram, NULL, cb)) {
			printf("Error testing anagram #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		gettimeofday(&tf, NULL);
		dt = delta(&tf, &ti);
		printf("\tIntegrity test successfully performed on %d permutations in %0.4f seconds.\n\n", anagram_permutation_count(anagram), dt);
	}

	/* filter permutation list */
	if (argc > 2) {
		printf("%d. Filtering permutation list using term \"%s\"...\n", seq++, argv[2]);
		gettimeofday(&ti, NULL);
		if ((c = anagram_filter(anagram, argv[2])) < 0) {
			printf("Error filtering permutation list #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		gettimeofday(&tf, NULL);
		dt = delta(&tf, &ti);
		if (c != anagram_count(anagram)) {
			printf("Error checking filtering result #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		printf("\t%d permutations selected out of %d in %0.4f seconds.\n\n", anagram_count(anagram), anagram_permutation_count(anagram), dt);
	}

	/* build text file path */
	sprintf(buf, "%s.txt", anagram_source_string(anagram));

	/* generate text file */
	printf("%d. Generating \"%s\" text file...\n", seq++, buf);

	/* open text file */
	fp = fopen(buf, "w");
	if (fp == NULL) {
		printf("Error opening text file #%04d\n", errno);
		exit(EXIT_FAILURE);
	}

	/* write text file header */
	i = fprintf(
		fp,
		"Anagram: \"%s\" (%d elements, %d bytes), Permutations: %d\nFilter: \"%s\", Matches: %d\n\n",
		anagram_source_string(anagram),
		anagram_element_count(anagram),
		(int)strlen(anagram_source_string(anagram)),
		anagram_permutation_count(anagram),
		anagram_term(anagram),
		anagram_count(anagram)
	);
	if (i < 0) {
		printf("Error writing text file header #%04d\n", errno);
		exit(EXIT_FAILURE);
	}

	/* write file */
	gettimeofday(&ti, NULL);
	for (i = 0, c = anagram_count(anagram); i < c; i++)
		if (fprintf(fp, "%07d. %s\n", i + 1, anagram_string(anagram, i)) < 0) {
			printf("Error writing permutation to file #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
	gettimeofday(&tf, NULL);
	dt = delta(&tf, &ti);
	printf("\t%d permutations written to text file in %0.4f seconds.\n\n", i, dt);

	/* return filter to original state */
	if (argc > 2) {
		i = anagram_count(anagram);
		if ((c = anagram_filter(anagram, "")) < 0) {
			printf("Error reseting result set #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		if (c != anagram_permutation_count(anagram)) {
			printf("Error checking filtering reset #%04d\n", errno);
			exit(EXIT_FAILURE);
		}
		printf("...Result reset from %d to %d permutations.\n\n", i, c);
	}

	/* release anagram object */
	anagram_release(anagram);

	/* release allocated memory */
	free(buf);

	puts("Good-Bye!\n");

	exit(EXIT_SUCCESS);

}

float delta(struct timeval *b, struct timeval *a) {
	float dt = (b->tv_sec - a->tv_sec) + (b->tv_usec / 1000000.0f) - (a->tv_usec / 1000000.0f);
	return 	dt;
}

int cb(void *argument, int count, const char *anagram) {
	return 1;
}
