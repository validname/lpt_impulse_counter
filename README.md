# picd

picd - it's just a simple Linux daemon counts rising impulses on nACK pin of parallel port. But it also can measure an consumed amount of something (electricity, water, etc) if your impulse generator is something-meter. So, if you have an old-fashioned PC with old parallel port, it's still usefull.

## How can I use it?

- enable LPT in the BIOS and set 'bidirectional' or 'EPP' mode (which are using interrupts, such as IRQ7)
- disable loading ('blacklist') printer modules (such as lp, it registers self as exclusive device as the picd does)
- build and connect your hardware to nACK pin of parallel port
- clone this repository and build picd and utilities (`make all`)
- check nACK pin for impulses (`check_ack_polling`)
- check generation of interrupts on nACK pin (`check_ack_irq`)
- re-configure (mmmmm... see 'possible questions' below), install (`make install`) and run daemon

## Thanks

Actually, my code is mash of [this code](https://gist.github.com/havoc-io/11218666) and [this daemon skeleton](http://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux). I very appriciate you, guys!

## Hardware

Any scheme which switches nACK pin from ground to small current and back is good. Surely, I cannot explain how to build a scheme for any cases. But I can give a set of advices/contractions:
- you should know maximal/typical voltage of your impulse generator
- you should know maximal/typical current of your impulse generator
- you should know/detect polarity of your impulse generator
- you should know maximal/typical your current of your parallel port

And you see next section with my expirience.

## My case

My electricity meter has telemetric pins with open collector scheme also known as 'S0-pulse interface'. It behaves like a switch between full short and full break. This output has parameters:
- typical current is 10 mA (maximal is 30 mA)
- typical voltage is 12 V (maximal is 24 V)
- 3200 pulses counted in a hour is equal 1 KWt*H of consumed electricity

### Check S0 is working

I've built a simple scheme:
```
+5V >----[ 1.5 KOhm ]---- S0 pin1

GND >-------( A )-------- S0 pin2
```
Maximal current of this scheme is about 3.3 mA (5/1500) - less than even typical for my S0 output. 
Ampere meter shown short impulses of current, it was about 2.8 mA. And voltage on the S0 pins was about 0.8 V - less then typical. And I detect polarity of S0 outputs (direction of current: it cannnot flow in opposite direction because of p-n or n-p junction in the transistor.)

### Check nACK pin is working

I've connected nACK and D0 pins through the button key and just pressed it twice to ensure that nACK readings (from the status register of parallel port) is changed. (nACK meanse 'non-acknowlegde': besides this 'negative' mark, it isn't hardware inverted. It a just old meaning how it was used it in printing.) I've connected ampere meter in break of pins and measured current of 1.64 mA. It was less than typical current for my S0 output.

### Check nACK interrupts is generated

I've enabled interrupts for parallel port, ran simple script and just watched how interrupt counter is increased while I constantly press and release the button:
```
watch -n 1 "cat /proc/interrupts | grep parport"
```

### Build a scheme

So I've built a simple scheme:
```
  D0 >----[ 180 Ohm ]---- S0 pin1

nACK >------------------- S0 pin2
```
Resistor isn't nesessary, but if some human factor (it's me) burns out a parallel port and D0 will be connected on power, maximal current through S0 will be 28 mA (5/180), less than maximal - so my meter won't be damaged.
This scheme requires setting D0 to '0' (because dropped nACK pin reads as '1' and connected to D0 through shorten S0 reads as '0'). So it counts moments when 'switch' in the S0 is goes to off.

## You software and my scheme doesn't work together at all!

Let's check you parallel port.
- run `write_data_bits 00`
- run `check_ack_polling`
- short nACK pin with any of D0-D7 pin and checker will write "nACK pin signal is changed to 0" (that's because your parallel port is built on CMOS-logic and droppen pin is readed as '1' and connected to '0' pin is readed as '0')
- unshort your wire and checker will write "nACK pin signal is changed to 1"

If it doesn't happen, maybe your (very-very old) parallel port is built on TTL logic. Let's check it:
- run `write_data_bits ff`
- run `check_ack_polling`
- short nACK pin with any of D0-D7 pin and checker will write "nACK pin signal is changed to 1" (that's because droppen pin of TTL is readed as '0' and connected to '1' pin is readed as '1')
- unshort your wire and checker will write "nACK pin signal is changed to 0"

And if it doesn't happen too - sorry, I don't know why. This method of counting and this software doesn't help you.

## Possible questions

### Maybe, I have an electricity meter with pulse output (S0). Where can I read about it?

See http://openenergymonitor.org/emon/buildingblocks/introduction-to-pulse-counting and https://www.sbc-support.com/index.php?id=1460&tx_srcproducts_srcproducts%5Bfile%5D=Applicationnote_S0puls_output_EN_V1.1.pdf .

### How to read measured values?

```
# cat /var/log/picd.stats 
impulses        -       18287
elapsed_time    second  55529
last_period     microsecond     3110715
instant_frequency       Hz      0.32
instant_consumption     -       361.65
average_consumption     -       370.48
total_consumed  -       24423070.937500
```
It's a tab-delimited lines. First field is a name, second is a unit and third is a value.

### I run software, but it outputs: "Couldn't claim parallel port: No such device or address"

Check dmesg, maybe it has this lines:
```
[114958.160728] parport0: cannot grant exclusive access for device ppdev0
[114958.160735] ppdev0: failed to register device!
```
or this:
```
[116455.329514] parport0: no more devices allowed
[116455.329521] ppdev0: failed to register device!
```
If it has, check if lp or another printer driver is loaded. It depends from parport driver as ppdev does and conflicts with picd:
```
# lsmod | egrep '(par|lp[[:blank:]$])'
parport_pc             31981  1
lp                     13299  0
parport                40836  3 lp,ppdev,parport_pc
```
You have to unload (`rmmod lp`) it.

Or, maybe, you trying to run utilities while picd is running. All of them is using parallel port driver (parport) in the exclusive mode, so use any two of them simultaneously is impossilble.

### How to initialize my scheme.

Add to init.d-script command `write_data_bits <hexbyte>` before starting picd. It'll just write this byte to data pins.

### How set offset for total consumption?

Just write current consumption (from the meter) to the cache file:

```
# echo "24417.38" > /var/run/picd.cache
```
picd will read it before next flushing.

### How to re-configure picd?

I'm too lazy to implement reading of configuration files. So, you have to edit the beginning of picd.c file and recompile it:
```
$EDITOR picd.c
make clean
make all
make install
```

You may want to change this constants:
- parallel port device (PP_DEV_NAME)
- full path to file with measured values (STAT_FILE)
- full path to file with value of total consumption (CACHE_FILE)
- how many you consumed per hour, measured as N impulses (CONSUME_HOUR_FACTOR)
- how often write values into stats- and cachefiles  (FLUSH_INTERVAL)

### How to do a POST-query to get values in JSON/XML/HTML/whatever format?

No way, sorry.

I wrote a very simple daemon to close my needs and I don't want to grow it to a monster.

But if you implement a WEB-server on *plain C* (with forks and IPC) and do a pull request, I'll be glad to merge it. It's usefull, sometimes.
