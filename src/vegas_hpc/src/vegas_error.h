/* vegas_error.h */
 /** Error handling routines for vegas.
 */
#ifndef _VEGAS_ERROR_H
#define _VEGAS_ERROR_H


/** Some exit codes */
#define VEGAS_OK          0
#define VEGAS_TIMEOUT     1 // Call timed out 
#define VEGAS_ERR_GEN    -1 // Super non-informative
#define VEGAS_ERR_SYS    -2 // Failed system call
#define VEGAS_ERR_PARAM  -3 // Parameter out of range
#define VEGAS_ERR_KEY    -4 // Requested key doesn't exist
#define VEGAS_ERR_PACKET -5 // Unexpected packet size

#define DEBUGOUT 0 

#ifdef __cplusplus /* C++ prototypes */
extern "C" {
#endif

/** Call this to log an error message */
void vegas_error(const char *name, const char *msg);

/** Call this to log an warning message */
void vegas_warn(const char *name, const char *msg);

/** Call this instead of printf to get a time stamped output message */
void tprintf(const char *fmt, ...);

void tfprintf(FILE *stream, const char *format, ...);

#define printf tprintf
#define fprintf tfprintf

#ifdef __cplusplus /* C++ prototypes */
}
#endif

#endif
