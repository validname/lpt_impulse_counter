CC=gcc
CFLAGS=

LIBS=-lm

%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

all: check_ack_irq check_ack_polling picd write_data_bits

.PHONY: clean

clean:
	-rm check_ack_irq check_ack_polling picd write_data_bits 2>/dev/null
