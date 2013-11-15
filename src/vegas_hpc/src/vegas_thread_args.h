#ifndef _VEGAS_THREAD_ARGS_H
#define _VEGAS_THREAD_ARGS_H
/** Generic thread args type with input/output buffer
 * id numbers.  Not all threads have both a input and a
 * output.
 */
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
struct vegas_thread_args {
    int input_buffer;
    int output_buffer;
    int priority;
    int finished;
    pthread_cond_t finished_c;
    pthread_mutex_t finished_m;
    cpu_set_t cpuset;    
};
void vegas_thread_args_init(struct vegas_thread_args *a);
void vegas_thread_args_destroy(struct vegas_thread_args *a);
void vegas_thread_set_finished(struct vegas_thread_args *a);
int vegas_thread_finished(struct vegas_thread_args *a, 
        float timeout_sec);
        
struct KeywordValues
{
    const char *name;
    unsigned int value;
};

/// For parsing the thread priority/thread affinity configuration file.
void read_thread_configuration(struct KeywordValues *keywords);
/// Gets the value of the associated keyword. If not found, zero is returned.
unsigned int get_config_key_value(char *keyword, struct KeywordValues *keywords);
/// Converts an affinity bitmask to a cpu_set structure.
void mask_to_cpuset(cpu_set_t *cpuset, unsigned int mask);
/// For debugging
int cpuset_to_mask(cpu_set_t *cpuset);
        
#endif
