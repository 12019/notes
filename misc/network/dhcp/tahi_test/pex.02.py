#!/usr/bin/env python

# reboot
# lio_start
# reboot> Reboot NUT.
# reboot>     * Wait until NUT boot up completely.
# reboot>         Ex.) Wait until catch up login prompt.
# reboot> 
# reboot> Press Enter key for continue. 
# lio_stop

# confirm dhclient down (1st & last)
# lio_start
# dhcp6c> then press enter key.
# dhcp6c> If NUT has Global address, please remove it. 
# 1dhcp6c> Stop the DHCPv6 Client! 
# 1lio_stop

# release
# dhcp6c> Please Release Address and information:
# According to RFC3315, 
#    A client sends a Release message to the server
#    that assigned addresses to the client to 
#    indicate that the client will no longer use one
#    or more of the assigned addresses.
# dhcp6c> In order to make client sent Release message. 
#  then press enter key. 
# lio_stop

# down interface
# lio_start
# dhcp6c> Please down interface of  DHCPv6 Client:
#  The Client will be required to send Confirm message when the intaface is upped.
#  then press enter key. 
# lio_stop

# end test
# You have new mail.
# root@ucsc13:~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3315 #

import pexpect

tahi_fout = open('./tahi_fout', 'w')

# start tahi
tahi = pexpect.spawn ('ssh root@10.25.161.149')
tahi.expect_exact('Password')
tahi.sendline('v6eval')
tahi.expect_exact('root@ucsc13')
tahi.sendline('cd ~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3315')
tahi.expect_exact('rfc3315 #')
print(tahi.before)
tahi.logfile = tahi_fout

# start ddr

def run_expect(tahi, ddr):
        """
        return:
        0 dhclient -r
        1 dhclient -r
        2 dhclient -x
        """
	dhclient_up = False
	print "run expect"
	#tahi.sendline('./C_RFC3315_SolAdvReqReply.seq -pkt ./C_RFC3315_SolAdvReqReply.def -log 2.html -ti "Part A &#58; Valid Reply message in response to Request"')

	while True:
		index = tahi.expect_exact(['1lio_stop', 'lio_stop', 'rfc3315 #'], timeout=None)
		print "index ", index

		if index == 0:
			if dhclient_up:
				ddr.sendline('./dhc431 -6 -r -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
				ddr.expect_exact('-bash-3.00#', timeout=None)
				print ddr.before
				dhclient_up = False
				tahi.sendline('')
			else:
				tahi.sendline('')

		if index == 1:
			if -1 != tahi.before.find('Please start NUT (DHCPv6 Client) manually'):
				print "up dhclient"
				ddr.sendline('./dhc431 -6 -nw -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
				ddr.expect_exact('-bash-3.00#')
				print ddr.before
				dhclient_up = True

			elif -1 != tahi.before.find('Please down interface of'):
				ddr.sendline('./dhc431 -6 -x -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
				ddr.expect_exact('-bash-3.00#')
				print ddr.before
				dhclient_up = False

			elif -1 != tahi.before.find('Please up interface of'):
				ddr.sendline('./dhc431 -6 -nw -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
				ddr.expect_exact('-bash-3.00#')
				print ddr.before
				dhclient_up = True

			elif -1 != tahi.before.find('In order to make client sent Release message'):
				ddr.sendline('./dhc431 -6 -r -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
				ddr.expect_exact('-bash-3.00#', timeout=None)
				print ddr.before
				dhclient_up = False
			elif -1 != tahi.before.find('Reboot NUT'):
				pass
			else:
				raise Exception('"%" does not match' % tahi.before)

			tahi.sendline('')

		if index == 2:
			return

print "connecting ddr"
ddr = pexpect.spawn('ssh root@10.25.163.144')
ddr.expect_exact('Password:')
ddr.sendline('abc123')
ddr.expect_exact('-bash-3.00#')
print(ddr.before)
ddr.sendline('cd /ddr/var/home/sysadmin')
ddr.expect_exact('-bash-3.00#')
print(ddr.before)
print "hello"

# test 1
"""
tahi.sendline('./DHCPv6_CltInit.seq -pkt /dev/null -log 1.html -ti "Initialization"')
tahi.expect_exact('Press Enter key for continue.')
print(tahi.before)
tahi.sendline('')
tahi.expect_exact('root@ucsc13:~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3315 #')
print(tahi.before)
"""
# test 2
print "test 2"
tahi.sendline('./C_RFC3315_SolAdvReqReply.seq -pkt ./C_RFC3315_SolAdvReqReply.def -log 2.html -ti "Part A &#58; Valid Reply message in response to Request"')
run_expect(tahi, ddr)
"""
tahi.sendline('./C_RFC3315_SolAdvReqReply.seq -pkt ./C_RFC3315_SolAdvReqReply.def -log 2.html -ti "Part A &#58; Valid Reply message in response to Request"')
tahi.expect_exact('1lio_stop')
print(tahi.before)
tahi.sendline('')
tahi.expect_exact('lio_stop')
print(tahi.before)

print "up dhclient"
ddr.sendline('./dhc431 -6 -nw -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
ddr.expect_exact('-bash-3.00#')
print ddr.before

print "cont test"
tahi.sendline('')
tahi.expect_exact('1lio_stop')
print(tahi.before)
ddr.sendline('./dhc431 -6 -r -cf /etc/dhcp/dd_dhclient6.conf -sf /etc/dhclient6-ddr-linux -pf /var/run/dhclient-eth0b.pid eth0b')
ddr.expect_exact('-bash-3.00#', timeout=None)
tahi.sendline('')
tahi.expect_exact('rfc3315 #', timeout=None)
print(tahi.before)
"""

# test 3
tahi.sendline('./C_RFC3315_CnfReply.seq -pkt ./C_RFC3315_CnfReply.def -log 3.html -ti "Part B &#58; Valid Reply message in response to Confirm message"')
run_expect(tahi, ddr)
