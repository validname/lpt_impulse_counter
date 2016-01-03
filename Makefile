CC=gcc
CFLAGS=
LIBS=-lm

PREFIX=/usr/
BIN=$(PREFIX)/sbin/
DAEMON=picd

%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

all: check_ack_irq check_ack_polling write_data_bits $(DAEMON)

install:
	cp -v -t $(BIN) $(DAEMON)
	cp -v initd /etc/init.d/$(DAEMON)
	chmod +x /etc/init.d/$(DAEMON)

uninstall:
	rm $(BIN)/$(DAEMON)
	rm /etc/init.d/$(DAEMON)

.PHONY: clean

clean:
	-rm check_ack_irq check_ack_polling picd write_data_bits 2>/dev/null
