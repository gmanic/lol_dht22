/*
 *      dht22.c:
 *	Simple test program to test the wiringPi functions
 *	Based on the existing dht11.c
 *	Amended by technion@lolware.net
 +	Further amended by gmanic
 */

#include <wiringPi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "locking.h"

#define MAXTIMINGS 40

#define MAXCYCLES 2000

static int DHTPIN = 7;
static int dht22_dat[5] = {0,0,0,0,0};
static uint16_t raw[MAXTIMINGS*2];

// Global locking vars for proper cleanup in case of errors
int lockfd = 0; //initialize to suppress warning
int lock = 1;

// Global debug state
int dbg = 0;

// Register cleanup function to remove lock if it is set
void bye( void) {
  pullUpDnControl(DHTPIN, PUD_OFF);
  if(lock)
    close_lockfile(lockfd);
}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
  < 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

static int expectPulse(int lev) {
  int cnt=0;
  while (sizecvt(digitalRead(DHTPIN)) == lev) {
    if (++cnt >= MAXCYCLES)
      return 0; // Timeout error
  }
  return cnt;
}

static int read_dht22_dat()
{
  uint8_t i;
  int dht_sum, t1, t2;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

// set pullup to off, pull high first, the down for approx. 18 milliseconds
  pullUpDnControl(DHTPIN, PUD_OFF); // pullup off, in case it was set
  pinMode(DHTPIN, OUTPUT);    // Set pin to output
  digitalWrite(DHTPIN, HIGH); // Set pin to high - dht22's sleep level
  delayMicroseconds(40000);   // 40ms high to accomodate
  digitalWrite(DHTPIN, LOW);  // Pull pin to low to startup dht22
  delayMicroseconds(10000);   // 10ms low - datasheet: says min 1, max 18ms
  digitalWrite(DHTPIN, HIGH); // Set pin high again

// prepare to read the pin, set pullup
  pinMode(DHTPIN, INPUT);     // Set pin to input
  pullUpDnControl(DHTPIN, PUD_UP); // Set pullup to up
  delayMicroseconds(10); // wait 10us (considering overhead; datasheet 20-40us)

// 80ms low intro 1 expected
  if ( (t1=expectPulse(LOW)) == 0) {
    if (dbg)
      printf("Timeout for start signal low pulse\n");
    printf("Timeout occured, skipping\n");
    return 0;
  }

// 80ms high intro 2 expected
  if ( (t2=expectPulse(HIGH)) == 0) {
    if (dbg)
      printf("Timeout for start signal high pulse\n");
    printf("Timeout occured, skipping\n");
    return 0;
  }

// read 40 values, initial low, then high
  for ( i=0; i< MAXTIMINGS*2; i+=2 ) {
    raw[i] = expectPulse(LOW);
    raw[i+1] = expectPulse(HIGH);
  }

// Switch pullup off
  pullUpDnControl(DHTPIN, PUD_OFF);

// That's it with timing critical stuff
  if (dbg)
    printf("Measured cycles for 80ms intro - Low: %d, high: %d\n", t1, t2);

// Inspect values to determine high/low of bit
  for ( i=0; i< 40; ++i) {
    int lowVal = raw[2*i];
    int highVal = raw[2*i+1];
    if (dbg)
      printf("Low time %d, High time %d\n", lowVal, highVal);
    if ( (lowVal == 0) || (highVal == 0) ) {
      if (dbg)
        printf("A pulse [%d] had a timeout, nonusable\n", i);
      printf("Timeout occured, skipping\n");
      return 0;
    }
    dht22_dat[i/8] <<= 1;
    if (highVal > lowVal)
      dht22_dat[i/8] |= 1;
  }

// Debug only output
  if (dbg) {
    printf("Received: %d\n", dht22_dat[0]);
    printf("Received: %d\n", dht22_dat[1]);
    printf("Received: %d\n", dht22_dat[2]);
    printf("Received: %d\n", dht22_dat[3]);
    printf("Received CRC: %d\n", dht22_dat[4]);
    printf("CRC   Result: %d\n", (dht22_dat[0]+dht22_dat[1]+dht22_dat[2]+dht22_dat[3]) & 0xFF );
  }

// verify checksum in the last byte, convert data and printout if good
  if (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF) ) {
        float t, h;
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

    printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
    return 1;
  }
  else
  {
// CRC wrong
    printf("Data not good, skipping\n");
    dht_sum=(dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF;
    if (dbg)
      printf("transitions: %d, raw: %d %d %d %d, sum %d, checksum %d\n", i, dht22_dat[0], dht22_dat[1], dht22_dat[2], dht22_dat[3], dht_sum, dht22_dat[4]);
    return 0;
  }
}

int main (int argc, char *argv[]) {
  int tries = 100;

  atexit(bye);

  if (argc < 2)
    printf ("usage: %s <pin> (<tries> <lock> <debug>)\n" \
            "description: pin is the wiringPi pin number\n" \
            "using 7 (GPIO 4)\n" \
            "Optional: tries is the number of times to try to obtain a read (default 100)\n" \
            "          lock: 0 disables the lockfile \n" \
            "                (for running as non-root user)\n" \
            "          debug: 1 for debug output (default: 0)\n",argv[0]);
  else
    DHTPIN = atoi(argv[1]);

  if (argc >= 3)
    tries = atoi(argv[2]);

  if (tries < 1) {
    printf("Invalid tries supplied\n");
    exit(EXIT_FAILURE);
  }

  if (argc >= 4)
    lock = atoi(argv[3]);

  if (lock != 0 && lock != 1) {
    printf("Invalid lock state supplied\n");
    exit(EXIT_FAILURE);
  }

  if (argc >= 5)
    dbg = atoi(argv[4]);

  printf ("Raspberry Pi wiringPi DHT22 reader\nwww.lolware.net\n" \
          "amended by gmanic\n") ;

  if(lock)
    lockfd = open_lockfile(LOCKFILE);

  if (wiringPiSetup () == -1)
    exit(EXIT_FAILURE) ;

  if (setuid(getuid()) < 0)
  {
    perror("Dropping privileges failed\n");
    exit(EXIT_FAILURE);
  }

  while (read_dht22_dat() == 0 && --tries)
     delayMicroseconds(2000000); // wait at least 2 sec to retry, dht22 is slow

  return 0;
}
