/**
 * Copyright 2022 Frank Duerr
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <CRC16.h>

// Ringbuffer with 128 entries.
// Consumes less than 1 KB RAM and can buffer samples at about 50 Hz for more than 2 seconds. 
#define RINGBUFFERSIZE 128  /* must be a power of two for fast modulo */
#define RINGBUFFERMOD 0x7f  /* fast modulo RINGBUFFERSIZE through binary & */

// Different packet types to transport different data.
#define PKTTYPE_SAMPLES 0 /* samples packet */
#define PKTTYPE_ONEPPS 1  /* 1-pps calibration packet */

// Send sample packets with these many samples.
#define BATCHSIZE 10

// Maximum packet size
#define MAX_PKTSIZE 1500

// SLIP special character codes.
#define SLIP_END             0300    /* indicates end of packet */
#define SLIP_ESC             0333    /* indicates byte stuffing */
#define SLIP_ESC_END         0334    /* ESC ESC_END means END data byte */
#define SLIP_ESC_ESC         0335    /* ESC ESC_ESC means ESC data byte */

typedef struct {
    uint32_t data[RINGBUFFERSIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t n;
} ringbuffer_t;

typedef struct __attribute__((__packed__)) {
    uint16_t type;
    uint16_t payload_length;
} pkt_header_t;

typedef struct __attribute__((__packed__)) {
    uint16_t crcsum;
} pkt_trailer_t;

typedef struct {
    unsigned char buffer[MAX_PKTSIZE]; // header, payload, trailer
} pkt_t;

volatile ringbuffer_t rb_samples;
volatile ringbuffer_t rb_onepps;

volatile boolean ledstate;
volatile short wavecnt;

// 16 bit CRC-CCITT: polynomial 0x1021 with start value 0x0000.
CRC16 crc;

inline pkt_header_t *get_pkt_header(pkt_t *pkt)
{
    return ((pkt_header_t *) pkt);
}

inline unsigned char *get_pkt_payload(pkt_t *pkt) 
{
    return (&pkt->buffer[sizeof(pkt_header_t)]);  
}

inline pkt_trailer_t *get_pkt_trailer(pkt_t *pkt)
{
    pkt_header_t *pkt_header = get_pkt_header(pkt);
    size_t traileroffset = sizeof(pkt_header_t) + pkt_header->payload_length;
    return ((pkt_trailer_t *) &pkt->buffer[traileroffset]);
}

void setup() 
{
    Serial.begin(115200);

    rb_samples.head = rb_samples.tail = rb_samples.n = 0;
    rb_onepps.head = rb_onepps.tail = rb_onepps.n = 0;
     
    // We use the LED to signal a fatal error (constantly on) 
    // and to blink once per second after capturing 50 waves during normal operation.
    ledstate = false;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, ledstate);
    wavecnt = 0;
    
    // SAM3XE has three Timer Counters (TC0, TC1, TC2), each with three channels:
    // + TC0->TC_CHANNEL[0]...
    // + TC0->TC_CHANNEL[1]...
    // + etc.
    // We will use TC0, channel 0 (sometimes denoted as TC0) for sampling frequencies. 
    // We will use TC2, channel 0 (sometimes denoted as TC6) for sampling 1 pps calibration signals from GPS.

    // TC is clocked through the Power Management Controller (PMC). 
    // In order to write CMR below, we need to disable its write protection:
    // Bits 8-31: Write Protection Key (WPKEY) 0x54494D 
    // Bit 0: 0 for disabling write protection
    // Note: seems to work without this. 
    //REG_TC0_WPMR = 0x54494D00;
    //pmc_set_writeprotect(false);
    
    // We use TIOA0 (PB25, Arduino Due pin #2) as external trigger for wave sampling.
    // Set the pin to peripheral mode.
    // Disable PIO control of the pin via PIO Disable Register (PDR);
    // let peripheral control the pin.
    PIOB->PIO_PDR |= PIO_PDR_P25; 
    // Set pin to peripheral function B through AB Select Register
    // (cf. Table 36-4).
    PIOB->PIO_ABSR |= PIO_ABSR_P25;                       

    // We use TIOA6 (PC25, Arduino Due pin #5) as external trigger for 1 pps signal.
    // Set the pin to peripheral mode.
    // Disable PIO control of the pin via PIO Disable Register (PDR);
    // let peripheral control the pin.
    PIOC->PIO_PDR |= PIO_PDR_P25; 
    // Set pin to peripheral function B through AB Select Register
    // (cf. Table 36-4).
    PIOC->PIO_ABSR |= PIO_ABSR_P25;
    
    // Enable peripheral clock for Timer Counter 0, Channel 0.
    // PCER = Peripheral Clock Enable Register
    // For the PID number, refer to Table 9-1 (PID27 is TC0, Channel 0)
    PMC->PMC_PCER0 |= PMC_PCER0_PID27; 
    //pmc_enable_periph_clk(ID_TC0);
    
    // Enable peripheral clock for Timer Counter 2, Channel 0.
    // PCER = Peripheral Clock Enable Register
    // For the PID number, refer to Table 9-1 (PID 33 is TC2, Channel 0)
    PMC->PMC_PCER1 |= PMC_PCER1_PID33; 
    //pmc_enable_periph_clk(ID_TC6);
   
    // Disable clock through TC Clock Control Register (CCR) 
    // while configuring.  
    TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKDIS;                           
    TC2->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKDIS;
    
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
    TC2->TC_CHANNEL[0].TC_CMR = 
        TC_CMR_TCCLKS_TIMER_CLOCK1  |
        TC_CMR_ABETRG |
        TC_CMR_LDRA_RISING |
        TC_CMR_LDRB_FALLING;
        
    // Trigger interrupts when RA is loaded.
    TC0->TC_CHANNEL[0].TC_IER |= TC_IER_LDRAS;
    TC2->TC_CHANNEL[0].TC_IER |= TC_IER_LDRAS;

    // Configure interrupts.
    NVIC_DisableIRQ(TC0_IRQn);
    NVIC_DisableIRQ(TC6_IRQn);
    NVIC_ClearPendingIRQ(TC0_IRQn); // clear pending interrupts
    NVIC_ClearPendingIRQ(TC6_IRQn); // clear pending interrupts
    NVIC_SetPriority(TC0_IRQn, 0); // TC interrupt has highest priority
    NVIC_SetPriority(TC6_IRQn, 0); // TC interrupt has highest priority
    NVIC_SetPriority(SysTick_IRQn, 15); // sys-tick interrupts have lower priority
    NVIC_EnableIRQ(TC0_IRQn); // enable interrupts
    NVIC_EnableIRQ(TC6_IRQn); // enable interrupts

    // To avoid triggering an overrun error on the first load of RA or RB,
    // we clear the status register and load RA and RB. 
    // Read and clear status register SR. 
    uint32_t stat = TC0->TC_CHANNEL[0].TC_SR;
    stat = TC2->TC_CHANNEL[0].TC_SR;
    // Read RA and RB.
    uint32_t ra = TC0->TC_CHANNEL[0].TC_RA;
    ra = TC2->TC_CHANNEL[0].TC_RA;
    uint32_t rb = TC0->TC_CHANNEL[0].TC_RB;
    rb = TC2->TC_CHANNEL[0].TC_RB;
    
    // Enable clock through TC Clock Control Register (CCR) and software trigger.
    TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
    TC2->TC_CHANNEL[0].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
}

/**
 * Called for any catastrophic error.
 */
void die() 
{
    /*
    if (ledon)
        digitalWrite(LED_BUILTIN, HIGH);
    else
        digitalWrite(LED_BUILTIN, LOW);
      
    // Halt
    while (1);
    */
    
    // Let it crash: reset to get back into a safe state quickly.
    rstc_start_software_reset(RSTC);
}

/**
 * Interrupt handler for Timer Counter 0, Channel 0 (aka TC0).
 */
void TC0_Handler() 
{
    uint32_t stat; // status
    
    // Read status register SR (SR will be reset after read). 
    stat = TC0->TC_CHANNEL[0].TC_SR;

    // Check for load overrun (loading at least twice in a row without reading).
    if (stat & TC_SR_LOVRS) {
        // Lost some values because ISR handling is too slow.
        // This must never happen since it results in wrong interval measurements.
        // Let it crash, reset, and get back into a safe state.
        die();
    }

    // Interrupt fired by loading RA (LDRAS)?
    if (stat & TC_SR_LDRAS) {
        // Get timestamp counter value from RA.
        uint32_t ra = TC0->TC_CHANNEL[0].TC_RA;
        
        // Copy timestamp value to ring buffer.
        // The interrupt routine has exclusive access to the ring buffer structure,
        // and the following code block is effectively atomic. 
        if (rb_samples.n == RINGBUFFERSIZE) {
            // Consumer is too slow to process data produced here.
            // This must never happen since it results in wrong interval measurements.
            // Let it crash, reset, and get back into a safe state.
            die();
        }
        rb_samples.data[rb_samples.head] = ra;
        rb_samples.head = ((rb_samples.head+1) & RINGBUFFERMOD);
        rb_samples.n++;

        // Blink LED once per second.
        wavecnt++;
        if (wavecnt == 50) {
            digitalWrite(LED_BUILTIN, ledstate);
            ledstate = !ledstate;
            wavecnt = 0;
        }
    }
    
    if (stat & TC_SR_LDRBS) {
        uint32_t rb = TC0->TC_CHANNEL[0].TC_RB;
    }
}

/**
 * Interrupt handler for Timer Counter 2, Channel 0 (aka TC6).
 */
void TC6_Handler() 
{
    uint32_t stat; // status
    
    // Read status register SR (SR will be reset after read). 
    stat = TC2->TC_CHANNEL[0].TC_SR;

    // Check for load overrun (loading at least twice in a row without reading).
    if (stat & TC_SR_LOVRS) {
        // Lost some values because ISR handling is too slow.
        // This must never happen since it results in wrong interval measurements.
        // Let it crash, reset, and get back into a safe state.
        die();
    }

    // Interrupt fired by loading RA (LDRAS)?
    if (stat & TC_SR_LDRAS) {
        // Get timestamp counter value from RA.
        uint32_t ra = TC2->TC_CHANNEL[0].TC_RA;
        
        // Copy timestamp value to ring buffer.
        // The interrupt routine has exclusive access to the ring buffer structure,
        // and the following code block is effectively atomic. 
        if (rb_onepps.n == RINGBUFFERSIZE) {
            // Consumer is too slow to process data produced here.
            // This must never happen since it results in wrong interval measurements.
            // Let it crash, reset, and get back into a safe state.
            die();
        }
        rb_onepps.data[rb_onepps.head] = ra;
        rb_onepps.head = ((rb_onepps.head+1) & RINGBUFFERMOD);
        rb_onepps.n++;
    }
    
    if (stat & TC_SR_LDRBS) {
        uint32_t rb = TC2->TC_CHANNEL[0].TC_RB;
    }
}

/**
 * Send a packet of serial using SLIP protocol.
 * The following code is adapted from RFC 1055.
 */
void send_packet_slip(const void *data, size_t len)
{
    const unsigned char *d = (unsigned char *) data;
    
    // Send an initial END character to flush out any data that may
    // have accumulated in the receiver due to line noise.
    // This might result in zero length packets on receiver side,
    // which the receiver should drop.
    Serial.write(SLIP_END);

    // For each byte in the packet, send the appropriate character
    // sequence.
    while (len--) {
        switch (*d) {
            // If it's the same code as an END character, we send a
            // special two character code so as not to make the
            // receiver think we sent an END.
            case SLIP_END:
                Serial.write(SLIP_ESC);
                Serial.write(SLIP_ESC_END);
                break;
            // If it's the same code as an ESC character,
            // we send a special two character code so as not
            // to make the receiver think we sent an ESC.
            case SLIP_ESC:
                Serial.write(SLIP_ESC);
                Serial.write(SLIP_ESC_ESC);
                break;
            default:
                Serial.write(*d);
        }
        d++;
    }

    // Tell the receiver that we're done sending the packet.
    Serial.write(SLIP_END);
}

void loop() 
{
    pkt_t pkt_samples; // a packet with samples
    pkt_t pkt_onepps; // a packet with 1-pps calibration values
    // Directly write samples into packet.
    uint32_t *batch = (uint32_t *) get_pkt_payload(&pkt_samples);
    size_t nbatch = 0;

    boolean first_sample = true;
    boolean first_onepps = true;
    uint32_t ts_samples_old; // previous timestamp for calculating interval
    uint32_t ts_onepps_old;
    
    while (true) {
        // Busy waiting for data to be produced.
        // Safe since reading a single byte is atomic. 
        while (rb_samples.n == 0 && rb_onepps.n == 0);

        // Since n is a single byte, reading it is safe (atomic).
        if (rb_samples.n > 0) { 
            // There is at least one sample in the ringbuffer.
            uint32_t ts = rb_samples.data[rb_samples.tail];
            rb_samples.tail = ((rb_samples.tail+1) & RINGBUFFERMOD);
            // Get exclusive access to n, without concurrent access by interrupt routine
            // (perform atomic read-modify-write on n).
            noInterrupts();
            rb_samples.n--;
            interrupts();

            if (first_sample) {
                // First value -> cannot calculate interval
                ts_samples_old = ts;
                first_sample = false;
            } else {
                // Not first value -> can calculate interval
                uint32_t interval;
                if (ts_samples_old > ts) {
                    // Wrap around case
                    interval = 0xffffffff-ts_samples_old + ts;
                } else {
                    interval = ts-ts_samples_old;
                }
                ts_samples_old = ts;

                batch[nbatch++] = interval;
                if (nbatch == BATCHSIZE) {
                    // Batch is complete -> send packet
                       
                    // Fill in packet header.
                    pkt_header_t *pkt_header = get_pkt_header(&pkt_samples);
                    pkt_header->type = PKTTYPE_SAMPLES;
                    pkt_header->payload_length = BATCHSIZE*sizeof(uint32_t);

                    // Packet payload is already filled in (zero-copy; batch points to packet payload).
                    
                    // Fill in packet trailer.
                    pkt_trailer_t *pkt_trailer = get_pkt_trailer(&pkt_samples);
                    crc.add(pkt_samples.buffer, sizeof(pkt_header_t)+pkt_header->payload_length);
                    pkt_trailer->crcsum = crc.getCRC();

                    size_t pkt_len = sizeof(pkt_header_t) + sizeof(pkt_trailer_t) + pkt_header->payload_length;
                    send_packet_slip(pkt_samples.buffer, pkt_len);
            
                    crc.reset();
                    nbatch = 0;
                }
            }
        }

        // Since n is a single byte, reading it is safe (atomic).
        if (rb_onepps.n > 0) { 
            // There is at least one 1-pps measurement in the ringbuffer.
            uint32_t ts = rb_onepps.data[rb_onepps.tail];
            rb_onepps.tail = ((rb_onepps.tail+1) & RINGBUFFERMOD);
            // Get exclusive access to n, without concurrent access by interrupt routine
            // (perform atomic read-modify-write on n).
            noInterrupts();
            rb_onepps.n--;
            interrupts();

            if (first_onepps) {
                // First value -> cannot calculate interval
                ts_onepps_old = ts;
                first_onepps = false;
            } else {
                // Not first value -> can calculate interval
                uint32_t interval;
                if (ts_onepps_old > ts) {
                    // Wrap around case
                    interval = 0xffffffff-ts_onepps_old + ts;
                } else {
                    interval = ts-ts_onepps_old;
                }
                ts_onepps_old = ts;

                // Fill in packet header.
                pkt_header_t *pkt_header = get_pkt_header(&pkt_onepps);
                pkt_header->type = PKTTYPE_ONEPPS;
                pkt_header->payload_length = sizeof(interval);

                // Copy payload into packet.
                uint32_t *payload = (uint32_t *) get_pkt_payload(&pkt_onepps);
                *payload = interval;
                
                // Fill in packet trailer.
                pkt_trailer_t *pkt_trailer = get_pkt_trailer(&pkt_onepps);
                crc.add(pkt_onepps.buffer, sizeof(pkt_header_t)+pkt_header->payload_length);
                pkt_trailer->crcsum = crc.getCRC();

                size_t pkt_len = sizeof(pkt_header_t) + sizeof(pkt_trailer_t) + pkt_header->payload_length;
                send_packet_slip(pkt_onepps.buffer, pkt_len);
            
                crc.reset();
            }
        }
    }
}
