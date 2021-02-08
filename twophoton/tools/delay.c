#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FNAMELENGTH 200         /* length of file name buffers */
#define FNAMFORMAT "%200s"      /* for sscanf of filenames */
#define INBUFSIZE 8192          /* choose 8kbyte large input buffer */

#define RINGBFORDER 20          /* allows for > 1e6 tags between start and stop event */
#define RINGBUFSIZE (1 << RINGBFORDER)
#define RINGBFMASK (RINGBUFSIZE - 1)

void emit_timetag(int overflow,int chan,unsigned long long time) {
  fwrite(&overflow,sizeof(int),1,stdout); // emit the old events from chan1
  fwrite(&chan,sizeof(int),1,stdout);
  fwrite(&time,sizeof(unsigned long long),1,stdout);
}

/* error handling */
char *errormessage[] = {
    "No error.",
    "error parsing input file name",
    "error parsing output file name",
    "error parsing time interval",
    "cannot open input file",                                           /* 5 */
    "error reading pattern",
    "error opening output file",
    "error parsing bin number",
    "bin number must be positive.",
    "cannot malloc histogram buffer",                                   /* 10 */
    "error parsing start channel",
    "error parting stop channel",
    "error in start or stop channel (1-256)",
    "you need to specify two ranges (start:stop) for skip (-s)",
    "skip needs to be formed of positive integers",                     /* 15 */
    "you need to specify two ranges (start:stop) for range (-r)",
    "range needs to be formed of positive integers",
    "stoprange needs to be larger than startrange",
    "delay (-d) needs a positive integer as argument",
    "factor needs to be a floating point number"                        /* 20 */
};

int emsg(int code) {
  fprintf(stderr, "%s\n", errormessage[code]);
  return code;
};

int printhelp(void) {
  fprintf(stderr, "Parameters:\n");
  fprintf(stderr, "-i [infile] (if not given read from stdin))\n");
  fprintf(stderr, "-o [outfile] (if not given write to stdout))\n");
  fprintf(stderr, "-p XX:YY (start and stop-pattern)\n");  // what means stop???
  fprintf(stderr, "-d XX delay one axis by XX ps, only positive values \n");
  fprintf(stderr, "-h help (this option)\n");
  exit(0);
};

typedef struct rawevent {
  unsigned long long upper;             /* most significan word */
  unsigned long long lower;             /* least sig word */
} re;

int main(int argc, char *argv[]) {
  int opt;                              /* option parsing */
  char infilename[FNAMELENGTH] = "C:/Data/somenottoobad_3.dump";
  char outfilename[FNAMELENGTH] = "C:/Data/shifted.dump";
  FILE *inhandle;
  FILE *outhandle;

  struct rawevent inbuf[INBUFSIZE];     /* input buffer */
  unsigned long long stored_time = 0;
  unsigned long long current_time = 0;
  int match_pattern = 1;
  int current_pattern;
  int stored_chan;
  int chan = 1;
  int overflow;

  int zero = 0;
  int delay = 54000;
  int index, retval;                    /* variables to handle input buffer */

  unsigned long long *ringbftime;       /* stores the time */
  unsigned int ringposition;
  unsigned long long *ringbfpattern;    /* stores the pattern */
  unsigned int oldringposition;


/* open input file */
  if (infilename[0]) {
    // printf(infilename[0]);
    inhandle = fopen(infilename, "r");
    if (!inhandle) return -emsg(5);
  } else {
    inhandle = stdin;
  };

/* open output file */
  if (outfilename[0]) {
    outhandle = fopen(outfilename, "w");
    if (!outhandle) return -emsg(7);
  } else {
    outhandle = stdout;
  };

/* allocate ring buffer */
  ringbftime = (unsigned long long *)calloc(RINGBUFSIZE, sizeof(unsigned long long));
  if (!ringbftime) return -emsg(10);

  ringbfpattern = (unsigned long long *)calloc(RINGBUFSIZE, sizeof(unsigned long long));
  if (!ringbfpattern) return -emsg(10);

  ringposition = 0;         /* initialize ring index */
  oldringposition = 0;      /* initialize stack ring index */

    /* initial reading */
  while (0 < (retval = fread(inbuf, sizeof(struct rawevent), INBUFSIZE, inhandle))) {
    for (index = 0; index < retval; index++) {
      /* extract tag information */
      current_time = ((unsigned long long)inbuf[index].lower);
      chan = ((int)(inbuf[index].upper >> 32));     // channel number from 1-8
      current_pattern = 1<<(chan-1);                // bit-pattern from 0-255
      overflow = inbuf[index].upper >> 56;          // extract the overflow

      if (current_pattern & match_pattern) {
      // binary delay pattern matched
        ringbftime[ringposition & RINGBFMASK] = current_time + delay;
        // store delayed time in time ring
        ringbfpattern[ringposition & RINGBFMASK] = chan;
        // store corresponding channel in pattern ring
        ringposition++;
      }

      else {
      // pattern was not matched, e.g. a trigger / non-delayed event was found
        while ((current_time >= (stored_time = ringbftime[oldringposition & RINGBFMASK]))
                && (oldringposition != ringposition))
        {
          // what was the saved pattern in the ringbuffer?
          stored_chan = ringbfpattern[oldringposition & RINGBFMASK];
          emit_timetag(zero, stored_chan, stored_time);
          oldringposition++;            // update the index of already processed events
        }
        emit_timetag(overflow, chan, current_time); // write out the non-delayed event
      }
    }
  }

  fclose(inhandle);
  /* write out the remaining events from the ringbuffer in order */
  while (oldringposition != ringposition) {
    stored_time = ringbftime[oldringposition & RINGBFMASK];
    stored_chan = ringbfpattern[oldringposition & RINGBFMASK];
    emit_timetag(zero, stored_chan, stored_time);
    oldringposition++;
  }

  free(ringbftime);
  free(ringbfpattern);
  fclose(outhandle);
  return 0;
}
