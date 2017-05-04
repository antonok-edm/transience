#ifndef TRANSIENT_H
#define TRANSIENT_H

typedef struct transient {
	public:
		int start;
		int end;

		transient(int s, int e) {
			start = s;
			end = e;
		}
} transient;

#endif
