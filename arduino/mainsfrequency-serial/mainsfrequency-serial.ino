#include <CRC16.h>

#define RINGBUFFERSIZE 16
#define RINGBUFFERMOD 0x0f

// Different packet types to transport different data.
#define PKTTYPE_SAMPLES 0 /* samples packet */

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

volatile ringbuffer_t rb;

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

    rb.head = rb.tail = rb.n = 0;

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
    
    // Read status register SR (SR will be reset after read). 
    stat = TC0->TC_CHANNEL[0].TC_SR;

    // Check for load overrun (loading at least twice in a row without reading).
    if (stat & TC_SR_LOVRS) {
        // Lost some values because ISR handling is too slow.
        // This must never happen.
        die();
    }

    // Interrupt fired by loading RA (LDRAS)?
    if (stat & TC_SR_LDRAS) {
        // Get timestamp counter value from RA.
        uint32_t ra = TC0->TC_CHANNEL[0].TC_RA;
        
        // Copy timestamp value to ring buffer.
        // Reading rb.n is safe since reading a single byte is atomic.
        if (rb.n == RINGBUFFERSIZE) {
            // Consumer is too slow to process data produced here.
            // This must never happen.
            die();
        }
        rb.data[rb.head] = ra;
        rb.head = ((rb.head+1) & RINGBUFFERMOD);
        // Read-modify-write in the ISR is safe (atomic) since the ISR
        // cannot be interrupted.
        rb.n++;

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
    // Directly write samples into packet.
    uint32_t *batch = (uint32_t *) get_pkt_payload(&pkt_samples);
    size_t nbatch = 0;

    boolean first_value = true;
    uint32_t ts_old; // previous timestamp for calculating interval
    
    while (true) {
        // Busy waiting for data to be produced.
        // Safe since reading a single byte is atomic. 
        while (rb.n == 0);

        // There is at least one data item in the ringbuffer.
        uint32_t ts = rb.data[rb.tail];
        rb.tail = ((rb.tail+1) & RINGBUFFERMOD);
        // Disable interrupts for atomic read-modify-write (other reader/writer is ISR).
        noInterrupts();
        rb.n--;
        interrupts();

        if (first_value) {
            // First value -> cannot calculate interval
            ts_old = ts;
            first_value = false;
            continue;
        }

        // Not first value -> can calculate interval
        uint32_t interval;
        if (ts_old > ts) {
            // Wrap around case
            interval = 0xffffffff-ts_old + ts;
        } else {
            interval = ts-ts_old;
        }
        ts_old = ts;

        batch[nbatch++] = interval;
        if (nbatch == BATCHSIZE) {
            // Batch is complete -> send packet
                       
            // Fill in packet header.
            pkt_header_t *pkt_header = get_pkt_header(&pkt_samples);
            pkt_header->type = PKTTYPE_SAMPLES;
            pkt_header->payload_length = BATCHSIZE*sizeof(uint32_t);
            
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
