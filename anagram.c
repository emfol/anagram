/*
 * @file anagram.c
 * @author Emanuel Fiuza de Oliveira
 * @email efiuza87@gmail.com
 * @date Sun, 6 Jan 2013 15:28 -0300
 */


#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stream/stream.h"
#include "anagram.h"


#if INT_MAX < 0x7FFFFFFF
/*  16-bit integer plataform. Allows up to 5040 permutations only */
#define ANAGRAM_ELEMENT_LIMIT 7
/* 7 elements of 4 bytes each + null byte */
#define ANAGRAM_SIZE_LIMIT 29
#else
/* 32-bit or greater integer plataform. Long enough to allow up to 3,628,800
 * permutations. */
#define ANAGRAM_ELEMENT_LIMIT 10
/* 10 elements of 4 bytes each + null byte */
#define ANAGRAM_SIZE_LIMIT 41
#endif

/* first three records of two bytes each */
#define ANAGRAM_FILE_MINSIZE 6


/*
 * Basic Types
 */


struct anagram {
	stream *file;
	int    bytes;
	int    elements;
	int    permutations;
	int    complete;
	int    base;
	int    count;
	int    references;
	char   source[ANAGRAM_SIZE_LIMIT];
	char   term[ANAGRAM_SIZE_LIMIT];
	char   buffer[ANAGRAM_SIZE_LIMIT];
};


/*
 * Static Function Interface
 */


static void utf8_encode(char *string, int *offset, long code);
static long utf8_decode(const char *string, int *offset);
static int utf8_strlen(const char *string, int *size);
static void sort(long *elements, int length);
int permute(long *elements, int length);


/*
 * Interface Implementation
 */


int anagram_element_limit(void)
{
	return ANAGRAM_ELEMENT_LIMIT;
}


anagram_ref anagram_create(const char *path, const char *string)
{

	struct anagram a, *ap;
	int i, errn;

	/* initialize local storage */
	memset(&a, 0, sizeof(struct anagram));
	a.file = NULL;

	/* calculate sizes */
	a.elements = utf8_strlen(string, &a.bytes);
	if (a.elements < 2 || a.elements > ANAGRAM_ELEMENT_LIMIT
		|| a.bytes < 2 || a.bytes > ANAGRAM_SIZE_LIMIT - 1) {
		errn = EINVAL;
		goto failure;
	}

	/* copy source string */
	memcpy(a.source, string, a.bytes);

	/* create file */
	a.file = stream_open(path, "w+");
	if (a.file == NULL) {
		errn = errno;
		goto failure;
	}

	/* write first record */
	memcpy(a.buffer, a.source, a.bytes);
	if (stream_write(a.file, a.buffer, a.bytes) != a.bytes) {
		errn = errno;
		goto failure;
	}

	/* write second and third records */
	memset(a.buffer, 0, a.bytes);
	for (i = 0; i < 2; i++) {
		if (stream_write(a.file, a.buffer, a.bytes) != a.bytes) {
			errn = errno;
			goto failure;
		}
	}

	/* try to allocate space from heap */
	ap = malloc(sizeof(struct anagram));
	if (ap == NULL) {
		errn = errno;
		goto failure;
	}

	/* initialize reference count */
	a.references = 1;

	/* copy local data to heap */
	memcpy(ap, &a, sizeof(struct anagram));

	/* success */
	return ap;

	failure:
		if (a.file != NULL) {
			stream_close(a.file);
			unlink(path);
		}
		errno = errn;
		return NULL;

}


anagram_ref anagram_open(const char *path)
{

	struct anagram a, *ap;
	ldiv_t division;
	long size;
	int i, errn;

	/* initialize local storage */
	memset(&a, 0, sizeof(struct anagram));
	a.file = NULL;

	/* open file */
	a.file = stream_open(path, "r+");
	if (a.file == NULL) {
		errn = errno;
		goto failure;
	}

	/* populate buffer with file data
	 * reading ANAGRAM_SIZE_LIMIT - 1 ensures the buffer is null-byte terminated */
	size = stream_read(a.file, a.buffer, ANAGRAM_SIZE_LIMIT - 1);
	if (size < ANAGRAM_FILE_MINSIZE) {
		errn = size < 0 ? errno : EBADF;
		goto failure;
	}

	/* calculate sizes */
	a.elements = utf8_strlen(a.buffer, &a.bytes);
	if (a.elements < 2 || a.elements > ANAGRAM_ELEMENT_LIMIT
		|| a.bytes < 2 || a.bytes > ANAGRAM_SIZE_LIMIT - 1) {
		errn = EINVAL;
		goto failure;
	}

	/* copy source string */
	memcpy(a.source, a.buffer, a.bytes);

	/* get file size */
	size = stream_end(a.file);

	/* check record count */
	division = ldiv(size, a.bytes);
	if (division.rem != 0 || division.quot < 3 || division.quot > INT_MAX) {
		errn = EBADF;
		goto failure;
	}

	/* save current permutation count
	 * total of records minus the first three control records */
	a.permutations = (int)division.quot - 3;

	/* point to second record */
	if (stream_seek(a.file, (long)a.bytes) < 0) {
		errn = errno;
		goto failure;
	}

	/* read second record to buffer */
	memset(a.buffer, 0, ANAGRAM_SIZE_LIMIT);
	if ((size = stream_read(a.file, a.buffer, a.bytes)) != a.bytes) {
		errn = size < 0 ? errno : EBADF;
		goto failure;
	}

	/* check if second record is zero filled */
	for (i = 0; i < a.bytes; i++) {
		if (*(a.buffer + i) != '\0') {
			errn = EBADF;
			goto failure;
		}
	}

	/* read third record to buffer */
	memset(a.buffer, 0, ANAGRAM_SIZE_LIMIT);
	if ((size = stream_read(a.file, a.buffer, a.bytes)) != a.bytes) {
		errn = size < 0 ? errno : EBADF;
		goto failure;
	}

	/* check if third record is zero filled (improve this?) */
	for (i = 0; i < a.bytes; i++) {
		if (*(a.buffer + i) != '\0') {
			a.complete = 1;
			break;
		}
	}

	/* set result */
	a.base = 0;
	a.count = a.permutations;

	/* try to allocate space from heap */
	ap = malloc(sizeof(struct anagram));
	if (ap == NULL) {
		errn = errno;
		goto failure;
	}

	/* initialize reference count */
	a.references = 1;

	/* copy local data to heap */
	memcpy(ap, &a, sizeof(struct anagram));

	/* success! */
	return ap;

	failure:
		if (a.file != NULL)
			stream_close(a.file);
		errno = errn;
		return NULL;

}


anagram_ref anagram_retain(anagram_ref a)
{
	if (a != NULL)
		a->references++;
	return a;
}


const char *anagram_source_string(anagram_ref a)
{
	if (a != NULL)
		return a->source;
	return NULL;
}


int anagram_element_count(anagram_ref a)
{
	if (a != NULL)
		return a->elements;
	return -1;
}


int anagram_permutation_count(anagram_ref a)
{
	if (a != NULL)
		return a->permutations;
	return -1;
}


int anagram_generate(anagram_ref a, void *argument, anagram_callback_f callback)
{

	stream *file;
	long size, element, elements[ANAGRAM_ELEMENT_LIMIT];
	int index, length, offset;
	int i, errn, canceled;
	char buffer[ANAGRAM_SIZE_LIMIT];
	const char *string;

	if (a == NULL) {
		errn = EINVAL;
		goto failure;
	}

	if (a->complete)
		goto success;

	/* initialize locals */
	file  = a->file;
	index = a->permutations;

	/* select source string or last generated permutation */
	if (index < 1) {
		index = 0;
		string = a->source;
	}
	else {
		if (stream_seek(file, ((long)index + 2) * a->bytes) < 0) {
			errn = errno;
			goto failure;
		}
		if ((size = stream_read(file, a->buffer, a->bytes)) != a->bytes) {
			errn = size < 0 ? errno : EBADF;
			goto failure;
		}
		a->buffer[a->bytes] = '\0';
		string = a->buffer;
	}

	/* populate array of elements */
	length = 0, offset = 0;
	while ((element = utf8_decode(string, &offset)) != 0) {
		if (element < 0) {
			errn = EILSEQ;
			goto failure;
		}
		elements[length++] = element;
	}

	/* check internal values */
	if (length != a->elements || offset != a->bytes) {
		errn = EBADF;
		goto failure;
	}

	/* set file offset */
	if (stream_seek(file, ((long)index + 3) * offset) < 0) {
		errn = errno;
		goto failure;
	}

	/* clear buffer */
	memset(buffer, 0, ANAGRAM_SIZE_LIMIT);

	/* sort elements if first permutation and write it to first record */
	if (index == 0) {
		sort(elements, length);
		for (i = 0, offset = 0; i < length; i++)
			utf8_encode(buffer, &offset, elements[i]);
		if (stream_write(file, buffer, offset) != offset) {
			errn = errno;
			goto failure;
		}
		a->permutations = ++index;
	}

	/* clear canceled flag */
	canceled = 0;

	/* perform permutations */
	while (permute(elements, length) != 0) {
		for (i = 0, offset = 0; i < length; i++)
			utf8_encode(buffer, &offset, elements[i]);
		if (stream_write(file, buffer, offset) != offset) {
			errn = errno;
			goto failure;
		}
		index++; /* point to next permutation */
		if (callback != NULL && !callback(argument, index, buffer)) {
			canceled = 1;
			break;
		}
	}

	/* set permutation count */
	a->permutations = index;

	/* update result set */
	a->base = 0;
	a->count = index;
	a->term[0] = '\0';

	if (canceled == 0) {
		if (stream_seek(file, (long)offset * 2) < 0) {
			errn = errno;
			goto failure;
		}
		if (stream_write(file, buffer, offset) != offset) {
			errn = errno;
			goto failure;
		}
		a->complete = 1;
	}

	/* flush changes to file */
	if (stream_sync(file) != 0) {
		errn = errno;
		goto failure;
	}

	success:
		return 1;

	failure:
		errno = errn;
		return 0;

}


int anagram_test(anagram_ref a, void *arg, anagram_callback_f cb)
{

	stream *file;
	long size;
	register int cnt, i, j;
	int len, errn;
	char bufa[ANAGRAM_SIZE_LIMIT], bufb[ANAGRAM_SIZE_LIMIT];

	if (a == NULL) {
		errn = EINVAL;
		goto failure;
	}

	if (a->permutations < 2 || !a->complete) {
		errn = EAGAIN;
		goto failure;
	}

	/* initialize locals */
	memset(bufa, 0, ANAGRAM_SIZE_LIMIT);
	memset(bufb, 0, ANAGRAM_SIZE_LIMIT);
	file = a->file;
	len = a->bytes;

	/* main comparison loop */
	for (i = 0, cnt = a->permutations; i < cnt - 1; i++) {
		if (stream_seek(file, ((long)i + 3) * len) < 0) {
			errn = errno;
			goto failure;
		}
		if ((size = stream_read(file, bufa, len)) != len) {
			errn = size < 0 ? errno : EBADF;
			goto failure;
		}
		for (j = i + 1; j < cnt; j++) {
			if ((size = stream_read(file, bufb, len)) != len) {
				errn = size < 0 ? errno : EBADF;
				goto failure;
			}
			if (strcmp(bufa, bufb) == 0) {
				errn = EILSEQ;
				goto failure;
			}
			if (cb != NULL && !cb(arg, j, bufb)) {
				errn = ECANCELED;
				goto failure;
			}
		}
	}

	return 1;

	failure:
		errno = errn;
		return 0;

}


const char *anagram_string(anagram_ref a, int index)
{

	long size;
	int errn;

	if (a == NULL) {
		errn = EINVAL;
		goto failure;
	}

	if (index < 0 || index >= a->count) {
		errn = ERANGE;
		goto failure;
	}

	if (stream_seek(a->file, ((long)index + (long)a->base + 3L) * (long)a->bytes) < 0) {
		errn = errno;
		goto failure;
	}

	if ((size = stream_read(a->file, a->buffer, a->bytes)) != a->bytes) {
		errn = size < 0 ? errno : EBADF;
		goto failure;
	}

	*(a->buffer + a->bytes) = '\0';

	return a->buffer;

	failure:
		errno = errn;
		return NULL;

}


int anagram_is_complete(anagram_ref a)
{
	if (a != NULL)
		return a->complete;
	return 0;
}


int anagram_filter(anagram_ref a, const char *s)
{

	stream *file;
	long size;
	int base, count, length, permutations;
	int i, errn;
	char term[ANAGRAM_SIZE_LIMIT], buf[ANAGRAM_SIZE_LIMIT];

	/* check for null pointers */
	if (a == NULL) {
		errn = EINVAL;
		goto failure;
	}

	/* if term is a null pointer or an empty string, reset result set */
	if (s == NULL || *s == '\0') {
		base = 0;
		count = a->permutations;
		term[0] = '\0';
		goto success;
	}

	/* check filter string */
	length = utf8_strlen(s, NULL);
	if (length < 1 || length > a->elements) {
		base = 0;
		count = 0;
		term[0] = '\0';
		goto success;
	}

	/* initialize local storage */
	file = a->file;
	length = a->bytes;
	permutations = a->permutations;
	base = 0;
	count = 0;
	strcpy(term, s);
	memset(buf, 0, ANAGRAM_SIZE_LIMIT);

	/* set stream pointer to first record */
	if (stream_seek(file, (long)length * 3) < 0) {
		errn = errno;
		goto failure;
	}

	/* perform search */
	for (i = 0; i < permutations; i++) {
		if ((size = stream_read(file, buf, length)) != length) {
			errn = size < 0 ? errno : EBADF;
			goto failure;
		}
		if (strstr(buf, term) == buf) {
			if (count == 0)
				base = i;
			count++;
		}
		else if (count != 0)
			break;
	}

	success:
		a->base = base;
		a->count = count;
		strcpy(a->term, term);
		return count;

	failure:
		errno = errn;
		return -1;

}


const char *anagram_term(anagram_ref a)
{
	if (a != NULL)
		return a->term;
	return NULL;
}


int anagram_count(anagram_ref a)
{
	if (a != NULL)
		return a->count;
	return -1;
}


void anagram_release(anagram_ref a)
{
	if (a == NULL)
		return;
	a->references--;
	if (a->references == 0) {
		stream_close(a->file);
		free(a);
	}
}


/*
 * Static Function Implementation
 */


static void utf8_encode(char *string, int *offset, long code)
{

	unsigned char *s;
	int i;

	if (code < 1)
		return;

	i = *offset;
	if (i < 0)
		return;

	s = (unsigned char *)(string + i);

	if (code < 0x80) { /* Up to U+007F */
		*s = (unsigned char)code;
		i++;
	}
	else if (code < 0x800) { /* Up to U+07FF */
		*s = (unsigned char)(0xC0 | (code >> 6));
		*(s + 1) = (unsigned char)(0x80 | (code & 0x3F));
		i += 2;
	}
	else if (code < 0x10000) { /* Up to U+FFFF */
		*s = (unsigned char)(0xE0 | (code >> 12));
		*(s + 1) = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
		*(s + 2) = (unsigned char)(0x80 | (code & 0x3F));
		i += 3;
	}
	else if (code < 0x110000) { /* Up to U+10FFFF */
		*s = (unsigned char)(0xF0 | (code >> 18));
		*(s + 1) = (unsigned char)(0x80 | ((code >> 12) & 0x3F));
		*(s + 2) = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
		*(s + 3) = (unsigned char)(0x80 | (code & 0x3F));
		i += 4;
	}

	*offset = i;

}


static long utf8_decode(const char *string, int *offset)
{

	unsigned char byte;
	int i, j;
	long code;

	i = *offset;
	if (i < 0)
		goto failure;

	byte = *(string + i++);

	if (byte < 0x80) {
		code = byte;
		j = 0;
	}
	else if ((byte & 0xE0) == 0xC0) {
		code = byte & 0x1F;
		j = 1;
	}
	else if ((byte & 0xF0) == 0xE0) {
		code = byte & 0x0F;
		j = 2;
	}
	else if ((byte & 0xF8) == 0xF0) {
		code = byte & 0x07;
		j = 3;
	}
	else
		goto failure;

	while (j > 0) {
		byte = *(string + i++);
		if ((byte & 0xC0) != 0x80)
			goto failure;
		code = (code << 6) | (byte & 0x3F);
		j--;
	}

	if (code != 0)
		*offset = i;

	return code;

	failure:
		return -1;

}


static int utf8_strlen(const char *string, int *size)
{

	int length, i, j;
	unsigned char byte;

	i = 0, length = 0;
	while ((byte = *(string + i)) != 0)
	{
		if (byte < 0x80)
			j = 0;
		else if ((byte & 0xE0) == 0xC0)
			j = 1;
		else if ((byte & 0xF0) == 0xE0)
			j = 2;
		else if ((byte & 0xF8) == 0xF0)
			j = 3;
		else
			goto failure;
		while (j > 0)
		{
			i++, j--;
			byte = *(string + i);
			if ((byte & 0xC0) != 0x80)
				goto failure;
		}
		i++, length++;
	}

	if (size != NULL)
		*size = i;

	return length;

	failure:
		if (size != NULL)
			*size = 0;
		return -1;

}


static void sort(long *elements, int length)
{

	/*
	 * Bubble Sort
	 */

	int swap, last, i;
	long temp;

	if (length < 2)
		return;

	last = length - 1;
	do {
		swap = 0;
		for (i = 0; i < last; i++) {
			if (elements[i] > elements[i + 1]) {
				temp = elements[i];
				elements[i] = elements[i + 1];
				elements[i + 1] = temp;
				swap = 1;
			}
		}
	} while (swap);

}


int permute(long *elements, int length)
{

	/*
	 * Implementation of the SEPA permutation algorithm
	 * by Jeffrey A. Johnson
	 */

	int key, nkey;
	long element;

	key  = length - 1;
	nkey = key;

	/* The key value is the first value from the end which
	 * is smaller than the value to its immediate right. [by J.A.J.] */

	while(key > 0 && elements[key] <= elements[key - 1])
		key--;

	key--;

	/* If key < 0 the data is in reverse sorted order,
	 * which is the last permutation. [by J.A.J.] */

	if(key < 0)
		return 0;

	/* elements[key + 1] is greater than elements[key] because of how key
	 * was found. If no other is greater, elements[key + 1] is used. [by J.A.J.] */

	element = elements[key];
	while(nkey > key && elements[nkey] <= element)
		nkey--;

	/* in place swap */
	/* element = elements[key]; */
	elements[key]  = elements[nkey];
	elements[nkey] = element;

	/* variables length and key are used to walk through the tail,
	 * exchanging pairs from both ends of the tail. length and
	 * key are reused to save memory. [by J.A.J.] */

	length--, key++;

	/* The tail must end in sorted order to produce the
	 next permutation. [by J.A.J.] */

	while(length > key) {
		/* in place swap */
		element = elements[length];
		elements[length] = elements[key];
		elements[key] = element;
		length--, key++;
	}

	return 1;

}

