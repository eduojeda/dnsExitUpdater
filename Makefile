dnsexitUpdate: dnsexitUpdate.o
	cc -o dnsexitUpdate dnsexitUpdate.o

clean:
	rm -f dnsexitUpdate *.o
