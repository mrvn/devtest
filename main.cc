/* Copyright (C) 2015 Goswin von Brederlow <goswin-v-b@web.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* test device for bad blocks and data corruption
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "pipe.h"
#include "worker.h"
#include "file.h"
#include "iocb.h"
#include "iothread.h"

void usage(const char *cmd) {
    printf("%s <options> <name>\n", cmd);
    printf("   --blocksize|-b <size>  Size of IO requests\n");
    printf("   --requests|-r <num>    Number of parallel requests\n");
    printf("   --memory|-m <size>     Amount of memory used for buffers\n");
    printf("   --workers|-w <num>     Number of worker threads\n");
}

class IOCBWorker : public Worker<IOCB, IOCB> {
public:
    IOCBWorker(ReadPipe<IOCB> in, WritePipe<IOCB> out)
	: Worker(std::move(in), std::move(out)) { }
private:
    IOCB * work(IOCB * iocb) {
	(*iocb)();
	return iocb;
    }
};

volatile bool print_completed = true;

void alarm_action(int, siginfo_t *, void *) {
    print_completed = true;
}

double diff(struct timeval & start, struct timeval & end) {
    return end.tv_sec - start.tv_sec
	+ (double(end.tv_usec) - start.tv_usec) / 1000000;
}

int main(int argc, char * const argv []) {
    size_t blocksize = 4096;
    int requests = 16;
    size_t memory = 0;
    int num_workers = 1;
    static const off_t MEGA = 1024 * 1024;

    while (true) {
	static struct option long_options[] = {
	    {"blocksize", required_argument, 0,  'b'},
	    {"memory",    required_argument, 0,  'm'},
	    {"requests",  required_argument, 0,  'r'},
	    {"workers",   required_argument, 0,  'w'},
	    {"help",      no_argument,       0,  'h'},
	    {0,           0,                 0,   0 },
	};
	int option_index = 0;

	int c = getopt_long(argc, argv, "b:hm:r:w:",
			    long_options, &option_index);
	if (c == -1)
	    break;
	switch (c) {
	case 'b':
	    blocksize = atoll(optarg);
	    break;
	case 'm':
	    memory = atoll(optarg);
	    break;
	case 'r':
	    requests = atoi(optarg);
	    break;
	case 'w':
	    num_workers = atoi(optarg);
	    break;
	case 'h':
	    usage(argv[0]);
	    exit(0);
	default:
	    fprintf(stderr, "Error: getopt returned character code %#x\n", c);
	    exit(1);
	}
    }

    if (optind == argc) {
	fprintf(stderr, "Error: filename missing\n");
	usage(argv[0]);
	exit(1);
    }

    const char *name = argv[optind];
    
    if (optind + 1 < argc) {
	printf("non-option ARGV-elements: ");
	while (optind < argc)
	    printf("%s ", argv[optind++]);
	printf("\n");
	exit(1);
    }

    if (memory == 0) memory = blocksize * requests;
    if (memory < blocksize * requests) {
	fprintf(stderr, "Error: memory [%lx] < blocksize * requests [%lx]\n",
		memory, blocksize * requests);
	exit(1);
    }

    printf("%s V0.0\n", argv[0]);
    printf("blocksize = %#lx\n", blocksize);
    printf("requests  = %d\n", requests);
    printf("memory    = %#lx\n", memory);
    printf("workers   = %d\n", num_workers);

    File file(name);
    off_t size = file.size() / blocksize * blocksize;
    if (size < off_t(memory)) {
	fprintf(stderr, "Error: Too much memory [%lx] for file size [%lx]\n",
		size, memory);
	exit(1);
    }

    static struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = alarm_action;
    action.sa_flags = SA_SIGINFO;
    int res = sigaction(SIGALRM, &action, nullptr);
    if (res != 0) {
	perror("sigaction");
	assert(false);
    }
    static struct itimerval val = {
	(struct timeval){1, 0},
	(struct timeval){1, 0},
    };
    res = setitimer(ITIMER_REAL, &val, nullptr);
    if (res != 0) {
	perror("setitimer");
	assert(false);
    }
    
    { // write test
	PipePair<IOCB> source = mkpipe<IOCB>();
	PipePair<IOCB> mid = mkpipe<IOCB>();
	PipePair<IOCB> drain = mkpipe<IOCB>();
    
	Workers<IOCBWorker> workers(num_workers, std::move(source.first),
				    std::move(mid.second));
	IOThread iothread(requests, std::move(mid.first),
			  std::move(drain.second));

	WritePipe<IOCB> in = std::move(source.second);
	ReadPipe<IOCB> out = std::move(drain.first);

	int num_iocb = memory / blocksize;
	off_t offset = 0;
	off_t completed = 0;
	off_t last_completed = 0;
	struct timeval start;
	struct timeval last;
	struct timeval now;

	res = gettimeofday(&start, nullptr);
	assert(res == 0);
	last = start;
	// fill pipe with buffers
	for (int i = 0; i < num_iocb; ++i) {
	    IOCB * iocb = new IOCB(file, IOCB::WRITE, blocksize);
	    iocb->offset(offset);
	    offset += blocksize;
	    in.write(iocb);
	}

	print_completed = false;

	// loop till end of file
	while (offset < size) {
	    IOCB * iocb = out.read();
	    iocb->check();
	    iocb->offset(offset);
	    offset += blocksize;
	    in.write(iocb);
	    completed += blocksize;
	    if (print_completed) {
		print_completed = false;
		res = gettimeofday(&now, nullptr);
		assert(res == 0);
		fprintf(stderr,
			"%f : write completed = %lu MiB / %lu MiB [ %f MiB/s ]\n",
			diff(start, now),
			completed / MEGA, size / MEGA,
			(completed - last_completed) / 1024.0 / 1024.0
			/ diff(last, now));
		last_completed = completed;
		last = now;
	    }
	}
	
	// reap buffers
	in.close();
	while (out) {
	    IOCB * iocb = out.read();
	    if (iocb == nullptr) {
		assert(!out);
		break;
	    }
	    iocb->check();
	    delete iocb;
	    completed += blocksize;
	    if (print_completed) {
		print_completed = false;
		res = gettimeofday(&now, nullptr);
		assert(res == 0);
		fprintf(stderr,
			"%f : write completed = %lu MiB / %lu MiB [ %f MiB/s ]\n",
			diff(start, now),
			completed / MEGA, size / MEGA,
			(completed - last_completed) / 1024.0 / 1024.0
			/ diff(last, now));
		last_completed = completed;
		last = now;
	    }
	}

	// final progress
	if (last_completed != completed) {
	    res = gettimeofday(&now, nullptr);
	    assert(res == 0);
	    fprintf(stderr,
		    "%f : write completed = %lu MiB / %lu MiB [ %f MiB/s ]\n",
		    diff(start, now),
		    completed / MEGA, size / MEGA,
		    (completed - last_completed) / 1024.0 / 1024.0
		    / diff(last, now));
	}
    }
    
    { // read test
	PipePair<IOCB> source = mkpipe<IOCB>();
	PipePair<IOCB> mid = mkpipe<IOCB>();
	PipePair<IOCB> drain = mkpipe<IOCB>();
    
	IOThread iothread(requests, std::move(source.first),
			  std::move(mid.second));
	Workers<IOCBWorker> workers(num_workers, std::move(mid.first),
				    std::move(drain.second));

	WritePipe<IOCB> in = std::move(source.second);
	ReadPipe<IOCB> out = std::move(drain.first);

	int num_iocb = memory / blocksize;
	off_t offset = 0;
	off_t completed = 0;
	off_t last_completed = 0;
	struct timeval start;
	struct timeval last;
	struct timeval now;

	res = gettimeofday(&start, nullptr);
	assert(res == 0);
	last = start;
	// fill pipe with buffers
	for (int i = 0; i < num_iocb; ++i) {
	    IOCB * iocb = new IOCB(file, IOCB::READ, blocksize);
	    iocb->offset(offset);
	    offset += blocksize;
	    iocb->fill();
	    in.write(iocb);
	}

	print_completed = false;

	// loop till end of file
	while (offset < size) {
	    IOCB * iocb = out.read();
	    iocb->offset(offset);
	    offset += blocksize;
	    iocb->fill();
	    in.write(iocb);
	    completed += blocksize;
	    if (print_completed) {
		print_completed = false;
		res = gettimeofday(&now, nullptr);
		assert(res == 0);
		fprintf(stderr,
			"%f : read completed = %lu MiB / %lu MiB [ %f MiB/s ]\n",
			diff(start, now),
			completed / MEGA, size / MEGA,
			(completed - last_completed) / 1024.0 / 1024.0
			/ diff(last, now));
		last_completed = completed;
		last = now;
	    }
	}
	
	// reap buffers
	in.close();
	while (out) {
	    IOCB * iocb = out.read();
	    if (iocb == nullptr) {
		assert(!out);
		break;
	    }
	    delete iocb;
	    completed += blocksize;
	    if (print_completed) {
		print_completed = false;
		res = gettimeofday(&now, nullptr);
		assert(res == 0);
		fprintf(stderr,
			"%f : read completed = %lu MiB / %lu MiB [ %f MiB/s ]\n",
			diff(start, now),
			completed / MEGA, size / MEGA,
			(completed - last_completed) / 1024.0 / 1024.0
			/ diff(last, now));
		last_completed = completed;
		last = now;
	    }
	}

	// final progress
	if (last_completed != completed) {
	    res = gettimeofday(&now, nullptr);
	    assert(res == 0);
	    fprintf(stderr,
		    "%f : read completed = %lu MiB / %lu MiB [ %f MiB/s ]\n",
		    diff(start, now),
		    completed / MEGA, size / MEGA,
		    (completed - last_completed) / 1024.0 / 1024.0
		    / diff(last, now));
	}
    }
    printf("shutting down\n");
}
