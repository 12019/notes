#! /usr/bin/env python
"""
Support for RFC 2136 dynamic DNS updates.
Requires dnspython/netaddr modules
"""
try:
	import dns.query
	import dns.tsigkeyring
	import dns.update
	import dns.reversename
	import netaddr
	dns_support = True
except ImportError as e:
	dns_support = False

import sys
import os

#JAMES: remove later
import logging

formatter = logging.Formatter('%(lineno)-4s %(funcName)-12s: %(message)s')

console_handler = logging.StreamHandler()
console_handler.setFormatter(formatter)

error_handler = logging.FileHandler("error.log", "w")
error_handler.setLevel(logging.ERROR)
error_handler.setFormatter(formatter)

log = logging.getLogger('ddns')
log.addHandler(console_handler)
#log.addHandler(error_handler)
log.setLevel(logging.ERROR)

# replace host-example with the appropriated value
keyring = dns.tsigkeyring.from_text({
    'rndc-key' : 'xsDJgO6KYgo7vVIArjsZ6Q=='
})

NS_FW_FILE = 'ns_ins_fw_file.txt'
NS_RV_FILE = 'ns_ins_rv_file.txt'

def __virtual__():
	"""
	Confirm dnspython is available
	"""
	if dns_support:
		return 'ddns'
	return False

def usage():
	"""
	The usage of this py

	./ddns.py cherry.local ipv6_ddr1 192.168.100.200 255.255.255.0 192.168.100.1
	
	"""
	print "update_dns.py <zone> <hostname> <ip> <mask> <dns server>"

def do_forward(cmd, zone, name, ip, nameserver, ttl, key='None', secret='None'):
	"""
	create a nsupdate command script for adding RRsets to the DNS

	ip4 nsupdate example:
		server 192.168.100.1
		key rndc-key xsDJgO6KYgo7vVIArjsZ6Q==
		zone cherry.local
		update delete ipv6_ddr1.cherry.local 30 IN A 192.168.100.83
		send
	ip6 example:
		server 2100:bad:cafe:f00d::1:101
		zone cherry.local
		update delete ipv6_ddr4.cherry.local 30 IN AAAA 2100:bad:cafe:f00d::100:100
		send
	"""
	ip_ver = '0'

	res = -1
	# using 'with open', we don't need to close the file.  Python will take care of it.
	with open(NS_FW_FILE, 'w') as f_ns:
		# ========== creating nsupdate instructions ==========
		f_ns.write("server %s\n" % format(nameserver))
		if not key == 'None':
			f_ns.write("key %s %s\n" % (key, secret))

		f_ns.write("zone %s\n" % format(zone))

		if ip.version == 4:
			ip_ver = 'A'
		else:
			ip_ver = 'AAAA'

		f_ns.write("update %s %s.%s %s IN %s %s\n" % (cmd, name, zone, ttl, ip_ver, str(ip.ip)))
		f_ns.write("send\n")

	res = os.system('nsupdate -v %s' % (NS_FW_FILE))
	# suceed: echo $? is 0
	# update failed: REFUSED => echo $? is 1
	# update failed: NOTAUTH => echo $? is 2
	if not (res == 0):
		log.error("nsupdate forward zone failed")

	return res

def do_reverse(cmd, zone, name, ip, nameserver, ttl, key='None', secret='None'):
	"""
	create a nsupdate command script for adding RRsets to the DNS

	ip4 nsupdate example:
		server 192.168.100.1
		key rndc-key xsDJgO6KYgo7vVIArjsZ6Q==
		zone 100.168.192.in-addr.arpa
		update delete 83.100.168.192.in-addr.arpa. 30 IN PTR ipv6_ddr1.cherry.local
		send
	ip6 example:
		server 2100:bad:cafe:f00d::1:101
		zone d.0.0.f.e.f.a.c.d.a.b.0.0.0.1.2.ip6.arpa.
		update delete 0.0.1.0.4.0.0.0.0.0.0.0.0.0.0.0.d.0.0.f.e.f.a.c.d.a.b.0.0.0.1.2.ip6.arpa. 30 IN PTR ipv6_ddr4.cherry.local
		send
	"""
	ip_ver = '0'
	res = -1
	ip4_total_dot = 4
	ip4_dot_per_byte = 8
	ip6_total_dot = 32
	ip6_dot_per_byte = 4

	# using 'with open', we don't need to close the file.  Python will take care of it.
	with open(NS_RV_FILE, 'w') as f_ns:
		# ========== creating nsupdate instructions ==========
		f_ns.write("server %s\n" % format(nameserver))
		if not key == 'None':
			f_ns.write("key %s %s\n" % (key, secret))

		if ip.version == 4:
			ip_ver = 'A'
			#rzone = "{0}".format(dns.reversename.from_address(str(ip.network)))
			#rzone = 'in-addr.arpa'
			prefixlen = ip.prefixlen

			#calculate the slot to avoid
			meaningful_dotted = prefixlen/ip4_dot_per_byte

			if (prefixlen % ip4_dot_per_byte) != 0:
				meaningful_dotted += 1

			# from 192.168.100.100 => ['0', '100', '168', '192']
			rip = str(ip.network).split('.')[::-1]

			# remove the 0 entry
			while (ip4_total_dot - meaningful_dotted) > 0:
				rip.pop(0)
				meaningful_dotted += 1
			rzone = '{0}.{1}'.format((".".join(rip)),"in-addr.arpa")
		
		else:
			ip_ver = 'AAAA'
			# example for the following line:
			# ip =  2001:db8:302::2/48
			# ip.network = '2001:db8:302::' =>  This is to get the prefix zone
			# dns.reversename.from_address/ip.network => this gets the reverse order
			#        = 0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.2.0.3.0.8.b.d.0.1.0.0.2.ip6.arpa.
			# dns.reversename.from_address/ip.network [:-1] => remove the last dot
			# print ip.network
			# print dns.reversename.from_address(str(ip.network))
			#rzone = "{0}".format(dns.reversename.from_address(str(ip.network)))[:-1]
			#rzone = 'ip6.arpa'

			prefixlen = ip.prefixlen

			#calculate the slot to avoid
			meaningful_dotted = prefixlen/ip6_dot_per_byte

			if (prefixlen % ip6_dot_per_byte) != 0:
				meaningful_dotted += 1

			# example:
			# ip = 2100:bad:cafe:fffd::1:101/64
			# ip.network = 2100:bad:cafe:fffd::	=> prefix
			# dns.reversename.from_address		=> 
			#		0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.d.f.f.f.e.f.a.c.d.a.b.0.0.0.1.2.ip6.arpa.
			rip = str(dns.reversename.from_address(str(ip.network))).split('.')
			# remove the 0 entry before the prefix
			while (ip6_total_dot - meaningful_dotted) > 0:
				rip.pop(0)
				meaningful_dotted += 1
			rzone = '{0}'.format((".".join(rip)))

		# James: need to double check if reverse zone is correct for IPv6
		# 0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa
		f_ns.write("zone %s\n" % format(rzone))
		f_ns.write("update %s %s %s IN PTR %s.%s\n" % (cmd, dns.reversename.from_address(str(ip.ip)), ttl, name, zone))
		f_ns.write("send\n")

	res = os.system('nsupdate -v %s' % (NS_RV_FILE))
	"""
	www.ietf.org/rfc/rfc2136.txt
	ddns rcode:
		0	NOERROR		success
		1	FORMERR		format error
		2	SERVFAIL	DNS encounters an internal error when handling the request
		3	NXDOMAIN	
		4	NOTIMP		The DNS doesn't support DDNS
		5	REFUSED		The DNS refuse to perform the request
		6	YXDOMAIN
		7	YXRRSET
		8	NXRRSET
		9	NOTAUTH		The DNS is not authoritative for the zone named in the Zone section
		10	NOTZONE		The RRSet is not within the zone denoted by the zone section.
	"""
	if (res != 0):
		log.error("nsupdate reverse zone failed")

	return res

def main():
	"""
	parameters:
	1/cmd:        it's the action taken for the script. (add, del, lookup)
	2/zone:       the domain name
	3/name:       the ddr hostname
	4/ip:         IP address w/ (prefixlen or mask) we want the ddns to update
	5/nameserver: dns ip
	6/ttl:        time to live value
	6/key:        TSIG key name
	7/secret:     TSIG secret

	example:
	IPv6
	./ddns_query.py add cherry.local ipv6_ddr1 2100:bad:cafe:f00d::1:101/64 192.168.100.1
	IPv4
	./ddns_query.py add cherry.local ipv6_ddr1 192.168.100.111/24 192.168.100.1 30 None
	./ddns_query.py del cherry.local ipv6_ddr1 192.168.100.111/255.255.255.0 192.168.100.1 30 rndc.key
	"""

	"""
	if (len(sys.argv) != 9):
		log.error("the len(argv) %d is wrong", len(sys.argv))
		log.error(sys.argv)
		usage()
		return
	"""
	cmd = sys.argv[1]
	zone = sys.argv[2]
	name = sys.argv[3]
	ip = netaddr.IPNetwork(sys.argv[4])
	nameserver = sys.argv[5]
	ttl = sys.argv[6]
	key = sys.argv[7]
	secret = sys.argv[8]

	log.info(" ".join([cmd, zone, name, str(ip), nameserver, ttl]))

	if os.path.isfile(NS_FW_FILE):
		log.info("instruction file exist: %s", NS_FW_FILE)
		os.remove(NS_FW_FILE)
	if os.path.isfile(NS_RV_FILE):
		log.info("instruction file exist: %s", NS_RV_FILE)
		os.remove(NS_RV_FILE)

	# check if nsupdate ins file exists, del it if exisits
	# create nsupdate ins file
	# create content
	if (cmd == 'add') or (cmd == 'delete'):
		# ================= Add/Del a RRset in the forward zone first =================
		if key == 'None':
			ret = do_forward(cmd, zone, name, ip, nameserver, ttl)
		else:
			ret = do_forward(cmd, zone, name, ip, nameserver, ttl, key, secret)

		if ret != 0:
			return ret

		# ================= Add/Del a RRset in the reverse zone =================
		if key == 'None':
			ret = do_reverse(cmd, zone, name, ip, nameserver, ttl)
		else:
			ret = do_reverse(cmd, zone, name, ip, nameserver, ttl, key, secret)

		if ret != 0:
			return ret

		return 0

	else:
		log.error("unsupported command")
		return -1

if __name__ == '__main__':
	main()
