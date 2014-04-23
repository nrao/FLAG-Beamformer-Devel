#ifndef BlankingStateMachine_h
#define BlankingStateMachine_h

enum BlankingState { NotBlanking, Blanking, WaitBlank};
class BlankingStateMachine
{
public:
    BlankingStateMachine();
    
    BlankingState get_state();

    const char *get_state_name();
        
    // Indicates whether the current FFT should be accumulated or blanked
    // non-zero value indicates the FFT should be blanked.
    int blank_current_fft();
    
    // This method takes the following encoding of bits:
    // 0x1 - Indicates whether the time series is blanked (anywhere)
    // 0x2 - Indicates if the time series is blanked at the start of the input
    // 0x4 - Indicates if the switching state changed
    void new_input(int blank_state_summary);
    
    // Tracks the switching status as time-series data comes in.
    // Returns non-zero when the sw_status does not match the previous value
    int sw_status_changed(int sw_status);
    
    // Returns non-zero if the contents of the GPU accumulator
    // should be flushed to the CPU due to a state/blanking change
    int needs_flush();
    
    void reset();
    
protected:
    void reset_blanking_cycle();
    BlankingState cur_state;
    BlankingState prev_state;
    int blanking_counter;
    int prev_sw_status;
    
};
#endif
