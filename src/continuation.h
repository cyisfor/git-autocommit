struct continuation {
	void (*func)(void*);
	void* arg;
};

void continuation_run(struct continuation c);
