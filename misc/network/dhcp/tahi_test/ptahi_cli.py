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
import time
from fabric.api import *

iface_dhcp = "ddsh -s net config eth0b dhcp yes ipversion ipv6"
iface_no_dhcp = "ddsh -s net config eth0b dhcp no"
iface_down = "ddsh -s net config eth0b down"
iface_up = "ddsh -s net config eth0b up"
ping_host = "ddsh -s net ping dhcpv6 ipversion ipv6 count 1 interface eth0b packet-size 2"
ping_domain = "ddsh -s net ping dhcpv6.test.example.com ipversion ipv6 count 1 interface eth0b packet-size 2"

test_3315 = "cd ~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3315"
test_3646 = "cd ~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3646"
test_3736 = "cd ~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3736"

end_3315 = "rfc3315 #"
end_3646 = "rfc3646 #"
end_3736 = "rfc3736 #"

# tahi test file
#tahi_test_file = "./py_tahi_3315_test_cases"
tahi_test_file = "./py_tahi_3646_test_cases"
# output file
tahi_output = "./zzz"
ddr_output = "./yyy"

def run_expect(tahi, ddr):
        """
        return:
        0 dhclient -r
        1 dhclient -r
        2 dhclient -x
        """
	dhclient_up = False
	do_dhcp_no = False

	while True:
		index = tahi.expect_exact(['1lio_stop', 'lio_stop', end_3315, end_3646, end_3736], timeout=None)
		# print "index ", index

		if index == 0:
			"""
			Before each testcase starts and ends, TAHI server will send the following msg:
				lio_start
				dhcp6c> then press enter key.
				dhcp6c> If NUT has Global address, please remove it. 
				1dhcp6c> Stop the DHCPv6 Client! 
				1lio_stop

			We don't want to do 'dhcp no' when starts the test since it's already no.
			However, we need to trun it off when exiting.
			"""
			print "James 1lio_stop"

			if do_dhcp_no:
				print "do dhcp no"
				ddr.sendline(iface_no_dhcp)
				ddr.expect_exact('done.', timeout=None)
				print ddr.before
				tahi.sendline('')
				do_dhcp_no = False
			else:
				print "do nothing"
				tahi.sendline('')

		if index == 1:
			"""
			Make sure handle test 11 where dhcp yes ipv6 fails
			"""
			print "James lio_stop"

			if -1 != tahi.before.find('Please start NUT (DHCPv6 Client) manually'):
				"""
				It takes ~ 4 seconds to bring up link local
				It takes ~ 10 seconds from dhclient bringup in CLI to TAHI gets request info.
				"""
				print "up dhclient"
				ddr.sendline(iface_dhcp)
				ddr.expect_exact('Configuring interface...')
					
				# we need to sleep a while for CLI to setup dhcp
				time.sleep(10)
				print ddr.before

				# need to tell Tahi to start sending IP info
				print "Ask Tahi to continue"
				tahi.sendline('')

				# if we fail to get DHCP, do_dhcp_no is set to 0/False. Otherwise, it's 1.
				print "wait for dhclient status"
				ret = ddr.expect_exact(['failed to get DHCPv6 lease', 'done.'], timeout=None)
				if ret == 0:
					do_dhcp_no = False
				else:
					do_dhcp_no = True

				dhclient_up = True

			elif -1 != tahi.before.find('Please down interface of'):
				print "down iface"
				ddr.sendline(iface_down)
				ddr.expect_exact('done.')
				print ddr.before
				dhclient_up = False

				tahi.sendline('')

			elif -1 != tahi.before.find('Please up interface of'):
				print "up iface"
				ddr.sendline(iface_up)
				ddr.expect_exact('Configuring interface...')
				time.sleep(10)
				print ddr.before
				dhclient_up = True
				tahi.sendline('')

			elif -1 != tahi.before.find('In order to make client sent Release message'):
				print "release iface"
				ddr.sendline(iface_no_dhcp)
				ddr.expect_exact('done.', timeout=None)
				print ddr.before
				do_dhcp_no = False
				dhclient_up = False
				tahi.sendline('')

			elif -1 != tahi.before.find('Reboot NUT'):
				print "reboot"
				tahi.sendline('')

			elif -1 != tahi.before.find("Destination= dhcpv6.test.example.com''"):
				print "ping dhcpv6.test.example.com"
				time.sleep(2)
				ddr.sendline(ping_domain)
				time.sleep(1)
				tahi.sendline('')
				ddr.expect_exact('unknown host', timeout=None)
				print ddr.before

			elif -1 != tahi.before.find("Destination= dhcpv6''"):
				print "ping dhcpv6.test.example.com"
				time.sleep(2)
				ddr.sendline(ping_domain)
				time.sleep(1)
				tahi.sendline('')
				ddr.expect_exact('unknown host', timeout=None)
				print ddr.before

			else:
				raise Exception('"%" does not match' % tahi.before)

		if index == 2:
			print "James test done"
			return
		if index == 3:
			return
		if index == 4:
			return

# start here
tahi_fout = open(tahi_output, 'w')
ddr_fout = open(ddr_output, 'w')

# start tahi
tahi = pexpect.spawn ('ssh root@10.25.161.149')
tahi.expect_exact('Password')
tahi.sendline('v6eval')
tahi.expect_exact('root@ucsc13')
#tahi.sendline(test_3315)
#tahi.expect_exact(end_3315)
tahi.sendline(test_3646)
tahi.expect_exact(end_3646)
print(tahi.before)
tahi.logfile = tahi_fout

# start ddr

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
ddr.logfile = ddr_fout

# set up fab, which is ddr's profile
env.user = 'root'
env.password='abc123'
env.host_string = '10.25.163.144'

# test 1
"""
tahi.sendline('./DHCPv6_CltInit.seq -pkt /dev/null -log 1.html -ti "Initialization"')
tahi.expect_exact('Press Enter key for continue.')
print(tahi.before)
tahi.sendline('')
tahi.expect_exact('root@ucsc13:~/Desktop/tahi/DHCP/DHCPv6_Self_Test_P2_1_1_4/rfc3315 #')
print(tahi.before)
"""
test_num = 1

with open(tahi_test_file, 'r') as tahi_tests:
#with open('./py_tahi_test3', 'r') as tahi_tests:
	for test in tahi_tests.readlines():
		print "========== TEST %d ==========" % test_num

		# print test[:-1]
		tahi.sendline(test[:-1])
		run_expect(tahi, ddr)

		# after we finish each test, we need to clean up DDR here.
		# let's run fab to kill dhclient6
		"""
		try: 
			run("kill -9 `pidof dhclient6`")
		except:
			print "kill error"

		try:
			print "removing lease file"
			run("rm /var/db/dhclient6.leases")
		except:
			print "lease removing error"
		
		if do_ddr_expect:
			print "test %d requires ddr expect" % test_num
			ddr.expect_exact(['done.','Configuring interface...',
				'failed to get DHCPv6 lease information in 60 seconds.'], timeout=None)
		"""
		test_num += 1

