volatile boolean ledstate;
volatile short wavecnt;

void setup() 
{
    // We use the LED to signal a fatal error and to blink once per second after
    // capturing 50 waves.
    ledstate = false;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, ledstate);
    wavecnt = 0;
    
    // SAM3XE has three Timer Counters (TC0, TC1, TC2), each with three channels:
    // + TC0->TC_CHANNEL[0]...
    // + TC0->TC_CHANNEL[1]...
    // + etc.
    // We will use TC0, channel 0 for sampling frequencies. 

    // We use TIOA0 (PB25, Arduino Due pin #2) as external trigger.
    // Set the pin to peripheral mode.
    // Disable PIO control of the pin via PIO Disable Register (PDR);
    // let peripheral control the pin.
    PIOB->PIO_PDR |= PIO_PDR_P25; 
    // Set pin to peripheral function B through AB Select Register
    // (cf. Table 36-4).
    PIOB->PIO_ABSR |= PIO_ABSR_P25;                       
    
    // Enable peripheral clock for Timer Counter 0, Channel 0.
    // Note: Here, ID_TC0, ID_TC1, etc. denote combinations of 
    // timer counters and channels as follows: 
    // ID_TC0 = Timer Counter 0, Channel 0 (this is what we want)
    // ID_TC1 = Timer Counter 1, Channel 1
    // etc.
    //pmc_enable_periph_clk(ID_TC0); 
    PMC->PMC_PCER0 |= PMC_PCER0_PID27; 

    // TC is clocked through the Power Management Controller (PMC). 
    // In order to write CMR below, we need to disable its write protection:
    // Bits 8-31: Write Protection Key (WPKEY) 0x54494D 
    // Bit 0: 0 for disabling write protection
    // Note: seems to work without this. 
    //REG_TC0_WPMR = 0x54494D00;
 
    // Disable clock through TC Clock Control Register (CCR) 
    // while configuring.  
    TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKDIS;                           
    
    // TC Channel Mode Register (CMR): Capture Mode (WAVE bit unset)
    // TC_CMR_TCCLKS_TIMER_CLOCK1: select internal MCK/2 clock signal
    // TC_CMR_ABETRG: TIOA is used as external trigger
    // TC_CMR_LDRA_RISING: load RA on rising edge of trigger input
    // TC_CMR_LDRB_FALLING: load RB on falling edge of trigger input
    TC0->TC_CHANNEL[0].TC_CMR = 
        TC_CMR_TCCLKS_TIMER_CLOCK1  |
        TC_CMR_ABETRG |
        TC_CMR_LDRA_RISING |
        TC_CMR_LDRB_FALLING;

    // Trigger interrupts when RA is loaded.
    TC0->TC_CHANNEL[0].TC_IER |= TC_IER_LDRAS;

    // Configure interrupts.
    NVIC_DisableIRQ(TC0_IRQn);
    NVIC_ClearPendingIRQ(TC0_IRQn); // clear pending interrupts
    NVIC_SetPriority(TC0_IRQn, 0); // TC interrupt has highest priority
    NVIC_SetPriority(SysTick_IRQn, 15); // sys-tick interrupts have lower priority
    NVIC_EnableIRQ(TC0_IRQn); // enable interrupts

    // To avoid triggering an overrun error on the first load of RA or RB,
    // we clear the status register and load RA and RB. 
    // Read and clear status register SR. 
    uint32_t stat = TC0->TC_CHANNEL[0].TC_SR;
    // Read RA and RB.
    uint32_t ra = TC0->TC_CHANNEL[0].TC_RA;
    uint32_t rb = TC0->TC_CHANNEL[0].TC_RB;
    
    // Enable clock through TC Clock Control Register (CCR) and software trigger.
    TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
}

/**
 * Called for any catastrophic error.
 */
void die() 
{
    // Signal error to user via LED.
    digitalWrite(LED_BUILTIN, HIGH);
    // Halt
    while (true);
}

/**
 * Interrupt handler for Timer Counter 0. 
 */
void TC0_Handler() 
{
    uint32_t stat; // status
    uint32_t ts; // timestamp
    
    // Read and clear status register SR. 
    stat = TC0->TC_CHANNEL[0].TC_SR;

    // Check for load overrun error (loading at least twice in a row without reading).
    if (stat & TC_SR_LOVRS) {
        // Lost some values because ISR handling is too slow.
        // This must never happen.
        die();
    }

    // Interrupt fired by loading RA (LDRAS)?
    if (stat & TC_SR_LDRAS) {
        // Get counter value from RA.
        ts = TC0->TC_CHANNEL[0].TC_RA;

        wavecnt++;
        if (wavecnt == 50) {
            digitalWrite(LED_BUILTIN, ledstate);
            ledstate = !ledstate;
            wavecnt = 0;
        }
    } 

    // Interrupt fired by loading RB (LDRBS)?
    if (stat & TC_SR_LDRBS) {
        uint32_t rb = TC0->TC_CHANNEL[0].TC_RB;
    }
}

void loop() 
{
    while (1);
}
