/*
 * @file anagram.h
 * @author Emanuel Fiuza de Oliveira
 * @email efiuza87@gmail.com
 * @date Sun, 6 Jan 2013 15:28 -0300
 */


#ifndef _ANAGRAM_H
#define _ANAGRAM_H


#include <stdlib.h>


/* Reference to anagram object opaque type. */
typedef struct anagram *anagram_ref;


/* Callback function to control time expensive functions */
typedef int (*anagram_callback_f)(void *argument, int count, const char *anagram);


/*
 * This function returns the maximum number of elements
 * an anagram is allowed to have.
 */
int anagram_element_limit(void);


/*
 * This function creates an anagram file on "path" using "string" as source.
 * On success, returns a reference to an anagram object. On failure,
 * returns a NULL pointer and sets errno to indicate the error.
 */
anagram_ref anagram_create(const char *path, const char *string);


/*
 * This function opens an anagram file. On success, returns a reference to an
 * anagram object. On failure, returns a NULL pointer and sets errno to indicate
 * the error.
 */
anagram_ref anagram_open(const char *path);


/*
 * This function increments the anagram object reference count and returns
 * the supplied anagram reference.
 */
anagram_ref anagram_retain(anagram_ref anagram);


/*
 * This function returns a pointer to the anagram source string. On failure,
 * a NULL pointer is returned.
 */
const char *anagram_source_string(anagram_ref anagram);


/*
 * This function returns the number of elements the supplied anagram object has.
 * On error, returns -1.
 */
int anagram_element_count(anagram_ref anagram);


/*
 * This function returns the number of permutations generated for the supplied
 * anagram object. If the function "anagram_generate" has not been called yet
 * it returns 0. On error, returns -1.
 */
int anagram_permutation_count(anagram_ref anagram);


/*
 * This function calculates all the possible permutations of the supplied
 * anagram object, one by one, and saves then on its backing file. If a callback
 * function is supplied, it is called each time a new permutation is generated
 * receiving "argument", the permutation index and the permuted string as
 * parameters. If the callback function returns 0, the generation process is
 * cancelled and it successfully returns. On success, it returns 1. On failure,
 * 0 is returned and errno is set indicate the error.
 */
int anagram_generate(anagram_ref anagram, void *argument, anagram_callback_f callback);


/*
 * This function checks the generated permutation list. On success, returns 1.
 * On failure, returns 0 and sets errno to indicate the error.
 */
int anagram_test(anagram_ref anagram, void *argument, anagram_callback_f callback);


/*
 * This function loads a permutation string from the last generated result set.
 */
const char *anagram_string(anagram_ref anagram, int index);


/*
 * This function checks if the list of permutations generated for the supplied
 * anagram object is complete (fully generated). 
 */
int anagram_is_complete(anagram_ref anagram);


/*
 * This function filters the entire list of permutations generated by
 * "anagram_generate" function and sets the anagram object result set.
 * On success, returns the number of permutations found. On error, returns -1
 * and sets errno to indicate the error.
 */
int anagram_filter(anagram_ref anagram, const char *term);


/*
 * This function returns a pointer to the last term string used to filter the
 * permutation list. On error, a null pointer is returned.
 */
const char *anagram_term(anagram_ref anagram);


/*
 * This function returns the number of permutations in the supplied anagram
 * object result set originated from the last call to "anagram_filter" function.
 * On success, returns the number of permutations in the last generated
 * result set. On error, returns -1 and sets errno to indicate the error.
 */
int anagram_count(anagram_ref anagram);


/*
 * Decrements the reference count of the supplied anagram object.
 * When the reference count reachs 0, the object is deallocated and
 * the backing file is closed. No value is returned.
 */
void anagram_release(anagram_ref anagram);


#endif
