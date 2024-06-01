#include <errno.h>
#include <getopt.h>
#include <mqueue.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_QUEUE "packager-out"
#define OUTPUT_QUEUE "plogger-out"
#define MAX_MSG_SIZE 600

char *outfile = NULL;
char buffer[MAX_MSG_SIZE];

int main(int argc, char **argv) {
    int c; // Holder for choice
    opterr = 0;

    /* Get command line options. */
    while ((c = getopt(argc, argv, ":o:")) != -1) {
        switch (c) {
        case 'o':
            outfile = optarg;
            break;
        case ':':
            fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            exit(EXIT_FAILURE);
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            exit(EXIT_FAILURE);
        default:
            fputs("Something went wrong. Please check 'use fetcher' to see example usage.", stderr);
            exit(EXIT_FAILURE);
        }
    }

    FILE *outstream;
    // No output file provided, print to stdout
    if (outfile == NULL) {
        outstream = stdout;
    }

    // File, try to open
    else {
        outstream = fopen(outfile, "w");
        if (outstream == NULL) {
            fprintf(stderr, "Could not open file '%s'.\n", outfile);
            return EXIT_FAILURE;
        }
    }

    // Try to open input message queue
    mqd_t in_q = mq_open(INPUT_QUEUE, O_RDONLY);
    if (in_q < 0) {
        fprintf(stderr, "Could not open input message queue '%s': %s\n", INPUT_QUEUE, strerror(errno));
        return EXIT_FAILURE;
    }

    // Try to open output message queue
    struct mq_attr q_attr = {0};
    q_attr.mq_maxmsg = 10;
    q_attr.mq_msgsize = MAX_MSG_SIZE;
    mqd_t out_q = mq_open(OUTPUT_QUEUE, O_CREAT | O_RDONLY, S_IWOTH | S_IRUSR, &q_attr);
    if (out_q == -1) {
        fprintf(stderr, "Could not create output queue '%s' with error: '%s'\n", OUTPUT_QUEUE, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Read and log packets forever
    size_t nbytes;
    for (;;) {

        // If we fail to receive a message, jump to the next loop iteration
        nbytes = mq_receive(in_q, buffer, MAX_MSG_SIZE, 0);
        if (nbytes == (size_t)-1) {
            fprintf(stderr, "Failed to receive input message: %s.\n", strerror(errno));
            continue;
        }

        // If we fail to write to file, just log it and make sure we pass the packet out
        if ((fwrite(buffer, 1, nbytes, outstream)) < nbytes) {
            fprintf(stderr, "Failed to write data to outstream: %s\n", strerror(errno));
        }

        if (mq_send(out_q, buffer, nbytes, 0) == -1) {
            fprintf(stderr, "Failed to write output to message queue: %s.\n", strerror(errno));
        }
    }

    return EXIT_SUCCESS;
}
