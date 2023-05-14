#include <stdlib.h>

#include "store.h"
#include "ticker.h"

int main(int argc, char *argv[]) {
    if(ticker())
	return EXIT_FAILURE;
    else
	return EXIT_SUCCESS;
}
