# need to give the per_cpu__kstat as the arg1
# use 'grep per_cpu__kstat ${path}/System.map-ddr385898' to get the value
#     ffffffff808d0950 D per_cpu__kstat
#     ls_cpustat 0xffffffff808d0950
define ls_cpustat
	set $v_runq = $arg0
	set $cpu = 0
	printf "kstat offset is %p\n", $v_runq
	set $cpu_pda_p = (struct x8664_pda *)((long)_cpu_pda[$cpu])

	# cpu_pda is valid for all NR_CPUS. 
	# So, we use it's data_offset to verify if a cpu it's online
	printf "          user     nice   system  softirq     irq     idle   iowait    steal\n"
	while $cpu_pda_p->data_offset != 0
		# printf "cpu_pda_p %p offset %p $kstat %p\n", $cpu_pda_p, $cpu_pda_p->data_offset, ($v_runq + $cpu_pda_p->data_offset)
		set $kstat = (struct kernel_stat *)($v_runq + $cpu_pda_p->data_offset)
		set $cpu_usage = (struct cpu_usage_stat)$kstat->cpustat
		printf "cpu%02d %8d %8d %8d %8d", $cpu, $cpu_usage->user, $cpu_usage->nice, $cpu_usage->system, $cpu_usage->softirq
		printf "%8d %8d %8d %8d\n", $cpu_usage->irq, $cpu_usage->idle, $cpu_usage->iowait, $cpu_usage->steal

		set $cpu = $cpu+1
		set $cpu_pda_p = (struct x8664_pda *)((long)_cpu_pda[$cpu])
	end
end

define ls_sirqstat
	set $v_runq = $arg0
	set $cpu = 0
	printf "kstat offset is %p\n", $v_runq
	set $cpu_pda_p = (struct x8664_pda *)((long)_cpu_pda[$cpu])

	# cpu_pda is valid for all NR_CPUS. 
	# So, we use it's data_offset to verify if a cpu it's online
	printf "      (hex) pendingC  servi_C pendingT start_ts  done_ts sum_time prev_sum\n"

	while $cpu_pda_p->data_offset != 0
		# printf "cpu_pda_p %p offset %p $kstat %p\n", $cpu_pda_p, $cpu_pda_p->data_offset, ($v_runq + $cpu_pda_p->data_offset)
		set $kstat = (struct kernel_stat *)($v_runq + $cpu_pda_p->data_offset)

		set $idx = 0
		printf "----- ----- -------- -------- -------- -------- -------- -------- --------\n"
		while $idx < 8
			if $idx == 0 
				printf "CPU%02d HI   ", $cpu
			end
			if $idx == 1
				printf "CPU%02d TIMER", $cpu
			end
			if $idx == 2
				printf "CPU%02d NETTX", $cpu
			end
			if $idx == 3
				printf "CPU%02d NETRX", $cpu
			end
			if $idx == 4
				printf "CPU%02d BLOCK", $cpu
			end
			if $idx == 5
				printf "CPU%02d TASKL", $cpu
			end
			if $idx == 6
				printf "CPU%02d SCHED", $cpu
			end
			if $idx == 7
				printf "CPU%02d HTIME", $cpu
			end

			set $sirq = (struct softirq_psstat)$kstat->sirqstat[$idx]
			printf " %8x %8x %8x", $sirq->marked_pending_count, $sirq->servicing_count, $sirq->marked_pending_ts
			printf " %8x %8x %8x %8x", $sirq->service_start_ts, $sirq->service_done_ts, $sirq->sum_time, $sirq->prev_sum_time

			# pay attention to the pending count # != serving count
			if $sirq->marked_pending_count == $sirq->servicing_count
			set $idx = $idx + 1
		end

		set $cpu = $cpu+1
		set $cpu_pda_p = (struct x8664_pda *)((long)_cpu_pda[$cpu])
	end
end

define num_of_cpus
	set $cpu = 0
	set $cpu_pda_p = (struct x8664_pda *)((long)_cpu_pda[$cpu])
end

# arg:
# 	number of cpus
define ls_net
	set $cpu = $arg0

	# ipv4
	# &tcp_prot, &tcp_orphan_count, tcp_death_row.tw_count, &tcp_sockets_allocated, &tcp_memory_allocated
	# UDP: inuse		&udp_prot
	# UDPLITE: inuse 	&udplite_prot
	# RAW: inuse 		&raw_prot
	# FRAG: inuse memory	ip_frag_nqueues, ip_frag_mem
	set $tcp_prot = &(tcp_prot)
	printf 
	set $idx = 0

	while $idx < $cpu
		printf "inuse %d\n", $tcp_prot->stats[$idx].inuse
	# ipv6: tcpv6_prot, udpv6_prot, udplitev6_prot, rawv6_prot, ip6_frag_nqueues, ip6_frag_mem
		set $idx = $idx + 1
	end
end

# protocol is defined in in.h
define print_protocol
	set $protocol = $arg0
	printf " protocol %3d ", $protocol

	if $protocol == 0
		printf "IP     "
	end
	if $protocol == 1
		printf "ICMP   "
	end
	if $protocol == 6
		printf "TCP    "
	end
	if $protocol == 17
		printf "UDP    "
	end
	if $protocol == 41
		printf "IPV6   "
	end
	if $protocol == 136
		printf "UDPLITE"
	end
	if $protocol == 255
		printf "RAW    "
	end
end

# family type defined in socket.h
define print_family_type
	set $type = $arg0
	printf " family %d ", $type

	if $type == 1
		printf "UNIX   "
	end
	if $type == 2
		printf "INET   "
	end
	if $type == 3
		printf "AX25   "
	end
	if $type == 10
		printf "INET6  "
	end
	if $type == 16
		printf "NETLINK"
	end
end

# sock type defined in net.h
define print_sock_type
	set $type = $arg0
	printf "SOCK %d ", $type

	if $type == 1
		printf "STREAM   "
	end
	if $type == 2
		printf "DGRAM    "
	end
	if $type == 3
		printf "RAW      "
	end
	if $type == 4
		printf "DRM      "
	end
	if $type == 5
		printf "SEQPACKET"
	end
	if $type == 10
		printf "PACKET   "
	end
end

### Debug code for kmem_bufctl array
#   1st: slabp
define show_bufctl
	set $slabp = (struct slab*)$arg0
	printf "show_bufctl $slabp %p\n", $slabp

	set $bufctl = (unsigned int *)($slabp + 1)
	# printf "slabp %p bufctl %p \n", $slabp, $bufctl
	printf "non-used object:\n"
	set $free = (unsigned int)$bufctl[$slabp->free]
	set $idx = $slabp->free
	while $free != 0xffffffff
		printf "%d %p %x \n", $idx, &$bufctl[$idx], $free
		set $idx = $free
		set $free = (unsigned int)$bufctl[$idx]
	end
end

# BUFCTL_END	ffffffff
# BUFCTL_FREE	fffffffe
# BUFCTL_ACTIVE fffffffd
# SLAB_LIMIT	fffffffc

# 1st: sk_buff_head
# 2nd: list end point
define ls_sk_buff_head
	set $sk_buff_head = (struct sk_buff_head*)$arg0
	set $end = $arg1

	printf "ls_sk_buff_head %p %p\n", $sk_buff_head, $end

	while $sk_buff_head != $end
		printf "skbh: %p lock %d\n", $sk_buff_head, $sk_buff_head.lock.tracker.owner

		set $sk_buff = (struct sk_buff*)$sk_buff_head
		printf "users %2d sock %p net_device %p\n", $sk_buff->users->counter, $sk_buff->sk, $sk_buff->dev

		set $sk_buff_head = (struct sk_buff_head*)$sk_buff_head->next
	end
end

# ls_slab_sock - list all the objects (sock) in the given slabp
# 1st: slabp
# 2nd: num of elements per slab
# 3rd: buffer_size
define ls_slab_sock
	set $slabp = (struct slab*)$arg0
	set $num = $arg1
	set $buf_size = $arg2

	printf "ls_slab_sock %p %d %d\n", $slabp, $num, $buf_size
	set $idx = 0

#	printf "sk: family state reuse prot *** "
#	printf "sock: state flags type\n"
	#while $idx < $slabp->inuse
	while $idx < $num
		set $sock = (struct sock*)($slabp->s_mem + $buf_size * $idx)

		set $socket = $sock->sk_socket
		# we use sk_socket to verify if sock is valid
		# state is defined in tcp_states.h
		# TCP_ESTABLISHED 1
		# TCP_SYN_SENT    2
		# TCP_CLOSE       7
		# TCP_CLOSE_WAIT  8
#		if $socket != 0
#			printf "%d %p", $idx, $sock
			printf "\nsk: family state reuse prot *** "
			printf "sock: state flags type\n"
			printf "   "
			printf "%7d ", $sock->__sk_common.skc_family
			printf "%5d ", $sock->__sk_common.skc_state
			printf "%5d ", $sock->__sk_common.skc_reuse
			printf "%4d ", $sock->sk_protocol

		if $socket != 0
			# state/type is defined in net.h
			# SS_UNCONNECTED 1
			# SS_CONNECTED   3
			# SOCK_STREAM    1
			# SOCK_DGRAM     2
			printf "          "
			printf "%5d ", $socket->state
			printf "%5d ", $socket->flags
			printf "%4d ", $socket->type
			printf "%p ", $socket->file
		end
		printf "sk:%p sock:%p\n", $sock, $socket

		if ($sock->sk_receive_queue->next != 0) && ($sock->sk_receive_queue->next != &($sock->sk_receive_queue))
			printf "------ receive queue ------\n"
			ls_sk_buff_head $sock->sk_receive_queue->next &($sock->sk_receive_queue)
		end
		if ($sock->sk_write_queue->next != 0) && ($sock->sk_write_queue->next != &($sock->sk_write_queue))
			printf "------ write queue ------\n"
			ls_sk_buff_head $sock->sk_write_queue->next &($sock->sk_write_queue)
		end
		if ($sock->sk_async_wait_queue->next != 0) && ($sock->sk_async_wait_queue->next != &($sock->sk_async_wait_queue))
			printf "------ async wait queue ------\n"
			ls_sk_buff_head $sock->sk_async_wait_queue->next &($sock->sk_async_wait_queue)
		end
		if ($sock->sk_error_queue->next != 0) && ($sock->sk_error_queue->next != &($sock->sk_error_queue))
			printf "------ error queue ------\n"
			ls_sk_buff_head $sock->sk_error_queue->next &($sock->sk_error_queue)
		end

		set $idx = $idx + 1
	end

	#show_bufctl $arg0
### Debug code for kmem_bufctl array

#	set $bufctl = (unsigned int *)($slabp + 1)
	# printf "slabp %p bufctl %p \n", $slabp, $bufctl
#	printf "non-used object:\n"
#	set $free = (unsigned int)$bufctl[$slabp->free]
#	set $idx = $slabp->free
#	while $free != 0xffffffff
#		printf "%d %p %x \n", $idx, &$bufctl[$idx], $free
#		set $idx = $free
#		set $free = (unsigned int)$bufctl[$idx]
#	end

#	printf "%d %p %x\n", $idx, &$bufctl[$idx], $free
	
end

# ls_slab_list: list all slab lists in the given kmem_cache
# 1st: kmem_cache
# 2nd: end point, the kmem_list element (slabs_full/slabs_free/slabs_partial addr)
# 3rd: num of elements per slab
# 4th: buffer_size
define ls_slab_list
	set $cur_slab = $arg0
	set $tail = $arg1
	set $num = $arg2
	set $buf_size = $arg3

	printf "ls_slab_list %p %p %d %d\n", $cur_slab, $tail, $num, $buf_size
	while $cur_slab != $tail
		set $slab = (struct slab*)$cur_slab
		printf "%p inuse %3d free %4d nodeid %2d\n", $slab, $slab->inuse, $slab->free, $slab->nodeid

		# memory corruption in the kernel dump.  we avoid walking through it.
		if ($slab == 0xffff8101013b2000) || ($slab == 0xffff8108fa1d4000)
			printf "$slab %p is corrupted   <<<<<<<<<<<<<<<<<\n", $slab
		else
			ls_slab_sock $slab $num $buf_size
		end
		set $cur_slab = $slab->list->next
	end

end

# ------------------------------------------------------------------------------
# usage: ls_struct_kcache <kmem_cache>
# warnings: check nodeid, change idx before use this func
# ------------------------------------------------------------------------------
# list content of a given kmem_cache
# $arg0: struct kmem_cache *
#
# dependency: call ls_slab_list() to list 
define ls_struct_kcache
	printf " ********  WARNING: please set $cpu & $idx manually ****************\n"
	printf "ls_struct_kcache %p\n", $arg0
	set $struct_kmem_cache = (struct kmem_cache*)$arg0
	printf "%p %s\n", $struct_kmem_cache, $struct_kmem_cache->name
	printf "num %12d slab_size %4d colour %4d colour_off %5d\n", $struct_kmem_cache->num, $struct_kmem_cache->slab_size, $struct_kmem_cache->colour, $struct_kmem_cache->colour_off
	printf "batchcount %5d limit %8d shared %4d buffer_size %4d\n", $struct_kmem_cache->batchcount, $struct_kmem_cache->limit, $struct_kmem_cache->shared, $struct_kmem_cache->buffer_size, 
	printf "gfpflags %7d dflags %7d gfporder %2d flags %10d\n", $struct_kmem_cache->gfpflags, $struct_kmem_cache->dflags, $struct_kmem_cache->gfporder, $struct_kmem_cache->flags
	printf "reciprocal_buffer_size %d slabp_cache %p\n", $struct_kmem_cache->reciprocal_buffer_size, $struct_kmem_cache->slabp_cache

	# revisit: need to get the cpu num from 'cpu_online_map', and calculate the bit
	printf "================================================\n"
	printf "    addr           cpu avail limit bct  th entry\n"
	set $cpu = 0
	while $cpu < 8
		set $array = (struct array_cache*)((long)$struct_kmem_cache->array[$cpu])
		printf "%p %2d %4d  %4d %4d %4d %p\n", $array, $cpu, $array->avail, $array->limit, $array->batchcount, $array->touched, $array->entry
		set $cpu = $cpu+1
	end

	printf "=====================================================================================\n"
	printf "        kmem_list3     free_obj free_limit part               full               free\n"

	# revisit: idex is the nodeid, need to find what the global is, and use it instead
	set $idx = 0	
	# get slab lists
	while $idx < 4
		set $kmem_list = (struct kmem_list3*)$struct_kmem_cache->nodelists[$idx]

		# revisit: for crash 6.0, we cant use $kmem_list->slabs_partial->next, need to redefine the variable
		printf "%3d %p %4ld     %4d       %p %p %p\n", $idx, $kmem_list, $kmem_list->free_objects, $kmem_list->free_limit, $kmem_list->slabs_partial->next, $kmem_list->slabs_full->next, $kmem_list->slabs_free->next

		printf "full list\n"
		if $kmem_list->slabs_full->next == &($kmem_list->slabs_full)
			printf "full list is empty\n"
		else
			ls_slab_list $kmem_list->slabs_full->next &($kmem_list->slabs_full) $struct_kmem_cache->num $struct_kmem_cache->buffer_size
		end

		printf "\nfree list\n"
		if $kmem_list->slabs_free->next == &($kmem_list->slabs_free)
			printf "free list is empty\n"
		else
			ls_slab_list $kmem_list->slabs_free &($kmem_list->slabs_free) $struct_kmem_cache->num $struct_kmem_cache->buffer_size
		end

		# walkthrough partial list
		printf "\nPartial list\n"
		if $kmem_list->slabs_partial->next == &($kmem_list->slabs_partial)
			printf "Partial list is empty\n"
		else
			ls_slab_list $kmem_list->slabs_partial->next &($kmem_list->slabs_partial) $struct_kmem_cache->num $struct_kmem_cache->buffer_size
		end

		set $idx = $idx + 1
	end 

end

# Walks through INET (IPV4) socks
# if we want to check other families, use net_proto_family to identify the targets first
define ls_inetsw
	set $sock_type_idx = 0
	set $inetsw_p = ((long)inetsw[$sock_type_idx])

	while $inetsw_p != 0
		# list: struct inet_protosw has list @ 0 offset.  No need to calculate
		set $protosw = (struct inet_protosw*)$inetsw_p
		while $protosw != &((long)inetsw[$sock_type_idx])
			printf "*****************************************\n"
			print_sock_type $sock_type_idx
			print_family_type $protosw->type
			print_protocol $protosw->protocol
			printf " prot %p\n", $protosw->prot

			# struct proto is defined in sock.h
			set $prot = (struct proto*)$protosw->prot
			printf "name %s slab %p\n", $prot->name, $prot->slab
			set $port_slab = (struct kmem_cache *)$prot->slab
			if $port_slab != 0
				ls_struct_kcache $port_slab
			end

			set $protosw = (struct inet_protosw*)$protosw->list->next
		end

		set $sock_type_idx = $sock_type_idx+1
		set $inetsw_p = ((long)inetsw[$sock_type_idx])
	end
end


# ------------------------------------------------------------------------------
# usage: ls_part_slab
# warnings: check nodeid, change idx before use this func
# ------------------------------------------------------------------------------
# all the kmem_cache are created via kmem_cache_create() & stores in cache_chain
define ls_part_slab
	# calculate list offset in kmem_cache
	set $list_kmem_cache = (long)(&((struct kmem_cache*)0)->next)
	set $head = &cache_chain
	set $cur = $head->next
	#printf "head is %p\n", $head
	#printf "cur is %p\n", $cur

	while $cur != $head
		# the starting addr of each kmem_cache = the next addr - offset
		set $kmem_cache = (struct kmem_cache*)(((long)$cur) - $list_kmem_cache)
		printf "===========================================================\n"
		printf "%p %s\n", $kmem_cache, $kmem_cache->name
		printf "num %12d slab_size %4d colour %4d colour_off %5d\n", $kmem_cache->num, $kmem_cache->slab_size, $kmem_cache->colour, $kmem_cache->colour_off
		printf "batchcount %5d limit %8d shared %4d buffer_size %4d\n", $kmem_cache->batchcount, $kmem_cache->limit, $kmem_cache->shared, $kmem_cache->buffer_size
		printf "gfpflags %7d dflags %7d gfporder %2d flags %10d\n", $kmem_cache->gfpflags, $kmem_cache->dflags, $kmem_cache->gfporder, $kmem_cache->flags
		printf "reciprocal_buffer_size %d slabp_cache %p\n", $kmem_cache->reciprocal_buffer_size, $kmem_cache->slabp_cache

		# set up index to walkthrough notelists => MAX_NUMNODES
		# the current size is 64.  use 'struct -o kmem_cache'

		# node id: it's the numa node.  check the code to verify what the max id is
		# node_online_map
		# p {struct pglist_data} node_data[0] & [1]
		set $idx = 0
		while $idx < 4
			set $kmem_list = $kmem_cache->nodelists[$idx]

			# walkthrough partial list
			if $kmem_list
				printf "======================================================================\n"
				printf "%d kmem_list3 %p free_obj %4ld free_limit %4d part %p full %p empty %p\n", $idx, $kmem_list, $kmem_list->free_objects, $kmem_list->free_limit, $kmem_list->slabs_partial->next, $kmem_list->slabs_full->next, $kmem_list->slabs_free->next
				set $cur_slab = $kmem_list->slabs_partial->next
				while $cur_slab != $kmem_list
					set $slab = (struct slab*)$cur_slab
					printf "%p inuse %3d free %3d nodeid %2d\n", $slab, $slab->inuse, $slab->free, $slab->nodeid
					set $cur_slab = $slab->list->next
				end
			end


			set $idx = $idx + 1
		end 
		printf "\n"
		set $cur = $cur->next
	end
end
