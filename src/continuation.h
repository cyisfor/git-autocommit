#ifndef CONTINUATION_H
#define CONTINUATION_H

struct continuation {
	void (*func)(void*);
	void* arg;
};

void continuation_run(struct continuation c);

#endif /* CONTINUATION_H */
