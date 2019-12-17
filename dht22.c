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

#define DEBUG FALSE

#define MAXTIMINGS 85
static int DHTPIN = 7;
static int dht22_dat[5] = {0,0,0,0,0};
static uint16_t raw[MAXTIMINGS];

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

static int read_dht22_dat()
{
  uint8_t laststate = HIGH;
//  uint8_t counter = 0;
  uint8_t j = 0, i;
  int dht_sum;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(500);
  digitalWrite(DHTPIN, LOW);
  delay(10);
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(5);
// prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    raw[i] = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      raw[i]++;
      delayMicroseconds(1);
      if (raw[i] == 10000) break;
    }
    laststate = sizecvt(digitalRead(DHTPIN));
    if (raw[i] == 10000) break;
  }

  for ( i=0; i< MAXTIMINGS; i++) {

    if (DEBUG) printf("Counter for transition %d: %d\n", i, raw[i]);
    // ignore first 3 transitions and every uneven number
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (DEBUG) printf("Counter for bit %d: %d\n", j, raw[i]);
      if ((raw[i] > 14) && (raw[i] < 200))
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) &&
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
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
    printf("Data not good, skip\n");
    dht_sum=(dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF;
    if (DEBUG) printf("transitions: %d, bitcount: %d, raw: %d %d %d %d, sum %d, checksum %d\n", i, j, dht22_dat[0], dht22_dat[1], dht22_dat[2], dht22_dat[3], dht_sum, dht22_dat[4]);
    return 0;
  }
}

int main (int argc, char *argv[])
{
  int lockfd = 0; //initialize to suppress warning
  int tries = 100;
  int lock = 1;

  if (argc < 2)
    printf ("usage: %s <pin> (<tries> <lock>)\ndescription: pin is the wiringPi pin number\nusing 7 (GPIO 4)\nOptional: tries is the number of times to try to obtain a read (default 100)\n          lock: 0 disables the lockfile (for running as non-root user)\n",argv[0]);
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

  printf ("Raspberry Pi wiringPi DHT22 reader\nwww.lolware.net\namended by gmanic\n") ;

  if(lock)
    lockfd = open_lockfile(LOCKFILE);

  if (wiringPiSetup () == -1)
    exit(EXIT_FAILURE) ;

  if (setuid(getuid()) < 0)
  {
    perror("Dropping privileges failed\n");
    exit(EXIT_FAILURE);
  }

  while (read_dht22_dat() == 0 && tries--)
  {
     delay(3000); // wait 1.8sec to refresh
  }

  delay(500);
  if(lock)
    close_lockfile(lockfd);

  return 0 ;
}
