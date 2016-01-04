CC=gcc
CFLAGS=
LIBS=-lm

PREFIX=/usr/
BIN=$(PREFIX)/bin/
SBIN=$(PREFIX)/sbin/
DAEMON=picd

%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

all: check_ack_irq check_ack_polling write_data_bits $(DAEMON)

install:
	cp -v -t $(SBIN) $(DAEMON)
	cp -v initd /etc/init.d/$(DAEMON)
	chmod +x /etc/init.d/$(DAEMON)
	cp -v -t $(BIN) check_ack_irq check_ack_polling write_data_bits

uninstall:
	rm $(SBIN)/$(DAEMON)
	rm /etc/init.d/$(DAEMON)
	rm $(BIN)/check_ack_irq $(BIN)/check_ack_polling $(BIN)/write_data_bits

.PHONY: clean

clean:
	-rm check_ack_irq check_ack_polling picd write_data_bits 2>/dev/null
