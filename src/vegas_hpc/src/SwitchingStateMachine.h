#ifndef SwitchingStateMachine_h
#define SwitchingStateMachine_h
#include <stdint.h>
#define MAX_PHASES 8
// Window to average switch_period_counts
#define NAVG_WINDOW 8

struct _SwitchingStateMachine
{
    int64_t cur_count;
    int64_t end_exposure_count;
    int64_t counts_per_exposure;
    int64_t last_sw_transition_count;
    int64_t last_exposure_count;
    int64_t last_count;
    int64_t approximate_counts_per_cycle;
    int32_t prior_phase_idx;
    int32_t prior_accum_id;
    int32_t nphases;
    int32_t sig_ref_table[MAX_PHASES];
    int32_t cal_table[MAX_PHASES];
    int32_t accumid_table[MAX_PHASES];
    int32_t switch_periods_per_exposure;
    int32_t cur_accumid;
    int32_t cur_phase_idx;
    int32_t cur_sw_cycle_number;
    int32_t lower_counts_per_cycle;
    int32_t upper_counts_per_cycle;
};

typedef struct _SwitchingStateMachine SwitchingStateMachine;

// Allocate and initialize a switching signal state machine
SwitchingStateMachine *
create_switching_state_machine(int32_t nphases, int32_t *sref, int32_t *cal, 
                               int32_t num_swperiods_per_exp, int64_t counts_per_exposure);

// destructor. If arg is null, then this is a no-op. Otherwise arg is free'd
void destroy_switching_state_machine(SwitchingStateMachine *);

// Set the indicated state int32_to the state machine
// returns 1 when the exposure is complete, zero otherwise
int32_t new_input_state(SwitchingStateMachine *, int32_t accumid, int64_t count);

#endif
