#include "standards.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "door.h"
#include "error.h"

#if 0
/* A door_id_t is a 64-bit unsigned integer, and does not need to be
 * portable.  Used by get_unique_id.
 */
typedef union
{
  struct {
    unsigned long pid_hash : 19;
    unsigned long time_hash : 31;
    unsigned short counter : 14;
  } bitfield;

  door_id_t value;
} id_bitfield_t;
#endif

static door_id_t get_unique_id (void)
/* Returns an identifier intended to be unique among all doors on the
 * system.  The current algorithm generates a 64-bit ID from the
 * following fields: the PID of the calling process (mod 2^19-1), the
 * number of seconds since the Epoch (mod 2^31), and a 14-bit counter
 * shared by all threads of this program.  It can therefore only generate
 * duplicates if: A) the system generates two simultaneous PIDs with the
 * same hash value (out of 524,787 possible), B) the system has an uptime
 * in excess of 68 years, and happens to recycle the PID and sequence
 * number during the one second in seven decades that would cause a
 * collision, or C) the program creates more than 16,384 doors in one
 * second, despite resource contention for the counter.
 *
 * A system-wide counter in the kernel might be simpler.
 */
{
  static const unsigned long long seq_period = 16384;
  static const unsigned long long time_period = 2147483648;
  static const unsigned long long pid_modulus = 524787;

  static unsigned short seq_count = 0;
  static pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;

  unsigned long long id;

  id = ((unsigned long long)getpid() % pid_modulus) << 45;
  id |= ((unsigned long long)time(NULL) % time_period) << 14;

  if ( 0 != pthread_mutex_lock(&seq_lock) )
    fatal_system_error(__FILE__, __LINE__, "pthread_mutex_lock");

  ++seq_count;
  seq_count %= seq_period;
  id |= (unsigned long long)seq_count;

  if ( 0 != pthread_mutex_unlock(&seq_lock) )
    fatal_system_error(__FILE__, __LINE__, "pthread_mutex_unlock");

  return id;
}

void* spawn_id ( void* unused )
/* Spawn a unique ID in dynamic, thread-local storage and print it to 
 * stdout.
 */
{
  printf("%016llx\n", get_unique_id() );
  fflush(stdout);

  return NULL;
}

int main(void)
{
  pthread_t thread_id[8];
  unsigned i;

  (void)get_unique_id();

  fork();

  for ( i = 0; i < 8; ++i )
    if ( 0 != pthread_create(&thread_id[i], NULL, spawn_id, NULL) )
      perror("pthread_create");

  for ( i = 0; i < 8; ++i )
    pthread_join( thread_id[i], NULL );

  return EXIT_SUCCESS;
}

