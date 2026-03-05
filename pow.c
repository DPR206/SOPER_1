#include "pow.h"

#define PRIME POW_LIMIT
#define BIG_X 435679812
#define BIG_Y 100001819

/**
 * @brief Funcion POW para resolver
 *
 * @param x numero entero
 * @return resultado de la funcion
 */
long int pow_hash(long int x) {
  long int result = (x * BIG_X + BIG_Y) % PRIME;
  return result;
}