#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

// ADC1 channel 6, GPIO34 
.set adc_channel, 6

// 2^adc_oversampling_factor_log is number of measurements
.set adc_oversampling_factor_log, 5
.set adc_oversampling_factor, (1 << adc_oversampling_factor_log)

/* Define variables, which go into .bss section (zero-initialized data) */
.bss

// ADC reading high threshold
.global high_threshold
high_threshold: .long 0

// low val for mic volume
.global lowVal
lowVal: .long 0

// high val for mic volume
.global highVal
highVal: .long 0

.global ADC_reading
ADC_reading:
.long 0

/* Code goes into .text section */
    .text
    .global entry
entry:
    //initialize lowVal = 4095
    move r1, 4095
    move r2, lowVal
    st r1, r2, 0
    
    //initialize highVal = 0
    move r1, 0
    move r2, highVal
    st r1, r2, 0
    
    stage_rst
    
    sample_loop:
    // take adc and store into r1
    adc r1, 0, adc_channel + 1
    // push lowVal into r2
    move r2, lowVal
    //load lowVal into r3
    ld r3, r2, 0
    // subtract lowVal - adcValue, r3 = r3 - r1
    sub r3, r3, r1
    // if lowVal = 4095 is smaller than sample, dont store it
    // else, store it
    jump skip_min_update, eq
    jump skip_min_update, ov
    //store sample value into lowVal
    st r1, r2, 0
    skip_min_update:
    // if we have skipped min update, then we go to check for max update!!
    
    //move highVal in r2
    move r2, highVal
    // load maxVal into r3
    ld r3, r2, 0
    // subtract the sample by the highVal, which is 0
    // sample - adcValue, r1 - r3
    sub r3, r1, r3
    // if highVal = 0 is greater than the adcValue, dont store it
    // else, store it
    jump skip_max_update, eq
    jump skip_max_update, ov
    st r1, r2, 0
    skip_max_update:
    // if we've skipped both, then we increase the loop counter
    stage_inc 1
    jumps sample_loop, adc_oversampling_factor, lt
    
    // and now we need to calculate the delta
    // populate r2 with highVal
    move r2, highVal
    // load it into the register
    ld r2, r2, 0
    // populate r3 with lowVal
    move r3, lowVal
    ld r3, r3, 0
    sub r0, r2, r3
    
    // now delta is in r0, and we store it
    move r2, ADC_reading
    st r0, r2, 0
    //jump thresh_comparison
    //thresh_comparison:
    
    //compare to threshold
    move r3, high_threshold
    ld r3, r3, 0
    sub r3, r3, r0
    jump wake_up, ov

/* value within range, end the program */
.global exit
exit:
halt

.global wake_up
wake_up:
/* Check if the system can be woken up */
READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
and r0, r0, 1
jump exit, eq

/* Wake up the SoC, end program */
wake
WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
jump exit
