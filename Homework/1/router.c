#include "skel.h"

#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <net/if_arp.h>
#include <ctype.h>
#include <stdbool.h>

struct route_table_entry
{
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} __attribute__((packed));

struct arp_entry
{
	//__u32 ip;
	uint32_t ip;
	uint8_t mac[6];
} __attribute__((packed));

uint16_t checksum(void *vdata, size_t length)
{
	// Cast the data pointer to one that can be indexed.
	char* data=(char*)vdata;

	// Initialise the accumulator.
	uint64_t acc=0xffff;

	// Handle any partial block at the start of the data.
	unsigned int offset=((uintptr_t)data)&3;
	if (offset) {
		size_t count=4-offset;
		if (count>length) count=length;
		uint32_t word=0;
		memcpy(offset+(char*)&word,data,count);
		acc+=ntohl(word);
		data+=count;
		length-=count;
	}

	// Handle any complete 32-bit blocks.
	char* data_end=data+(length&~3);
	while (data!=data_end) {
		uint32_t word;
		memcpy(&word,data,4);
		acc+=ntohl(word);
		data+=4;
	}
	length&=3;

	// Handle any partial block at the end of the data.
	if (length) {
		uint32_t word=0;
		memcpy(&word,data,length);
		acc+=ntohl(word);
	}

	// Handle deferred carries.
	acc=(acc&0xffffffff)+(acc>>32);
	while (acc>>16) {
		acc=(acc&0xffff)+(acc>>16);
	}

	// If the data began at an odd byte address
	// then reverse the byte order to compensate.
	if (offset&1) {
		acc=((acc&0xff00)>>8)|((acc&0x00ff)<<8);
	}

	// Return the checksum in network byte order.
	return htons(~acc);
}

int cmp_rtable(const void *a, const void *b)
{
	struct route_table_entry *e1 = (struct route_table_entry*) a;
	struct route_table_entry *e2 = (struct route_table_entry*) b;
	uint32_t prefix1 = ntohl(e1->prefix);
	uint32_t prefix2 = ntohl(e2->prefix);
	uint32_t mask1 = ntohl(e1->mask);
	uint32_t mask2 = ntohl(e2->mask);
	if(prefix1 == prefix2)
		return mask1 - mask2;
	return prefix1 - prefix2;
}
		

int main(int argc, char *argv[])
{
	packet m;
	int rc;

	init();

	// read route table
	FILE *fd = fopen("rtable.txt", "r");
	int rtable_entries = 0;
	char ch;
	while(!feof(fd))
	{
		ch = fgetc(fd);
		if(ch == '\n')
			rtable_entries++;
	}
	fseek(fd, 0, SEEK_SET);
	struct route_table_entry *rtable = malloc(sizeof(struct route_table_entry) * rtable_entries);
	char line[50];
	for(int i = 0; i < rtable_entries; ++i)
	{
		fgets(line, sizeof(line), fd);
		int j = 0;
		// prefix
		char *c = &line[j];
		while(line[j] != ' ')
			j++;
		line[j] = '\0';
		rtable[i].prefix = inet_addr(c);
		// next hop
		c = &line[++j];
		while(line[j] != ' ')
			j++;
		line[j] = '\0';
		rtable[i].next_hop = inet_addr(c);
		// mask
		c = &line[++j];
		while(line[j] != ' ')
			j++;
		line[j] = '\0';
		rtable[i].mask = inet_addr(c);
		// interface
		++j;
		int nr = 0;
		while(isdigit(line[j]))
		{
			nr = nr * 10 + line[j] - '0';
			j++;
		}
		rtable[i].interface = nr;
	}

	fclose(fd);

	// sort route table
	qsort(rtable, rtable_entries, sizeof(struct route_table_entry), cmp_rtable);

	// initialize ARP table
	struct arp_entry *arp_table = malloc(sizeof(struct arp_entry) * 4);
	int arp_table_size = 4;
	int arp_entries = 0;

	// initialize packets array
	packet **packets_to_be_sent = malloc(sizeof(packet*) * 16);
	int packets_array_size = 16;
	int nr_packets_to_be_sent = 0;
	
	while (1)
	{
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");

		struct ether_header *eth_hdr = (struct ether_header *)m.payload;
		// if ARP
		if(ntohs(eth_hdr->ether_type) == ETHERTYPE_ARP)
		{
			struct ether_arp *eth_arp = (struct ether_arp*)(m.payload + sizeof(struct ether_header));
			uint32_t ip;
			memcpy(&ip, eth_arp->arp_tpa, 4);
			// if ARP request to router, send reply
			if(ntohs(eth_arp->arp_op) == ARPOP_REQUEST && ip == inet_addr(get_interface_ip(m.interface)))
			{
				// ether_arp
				u_char mac[6];
				get_interface_mac(m.interface, mac);
				memcpy(eth_arp->arp_tha, eth_arp->arp_sha, 6);
				memcpy(eth_arp->arp_sha, mac, 6);
				__u32 addr;
				memcpy(&addr, eth_arp->arp_tpa, 4);
				memcpy(eth_arp->arp_tpa, eth_arp->arp_spa, 4);
				memcpy(eth_arp->arp_spa, &addr, 4);
				eth_arp->arp_op = htons(ARPOP_REPLY);
				// ether_header
				memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
				memcpy(eth_hdr->ether_shost, mac, 6);
				send_packet(m.interface, &m);
				continue;
			}
			// if ARP reply, update ARP table if necessary
			else if(ntohs(eth_arp->arp_op) == ARPOP_REPLY)
			{
				memcpy(&ip, eth_arp->arp_spa, 4);
				bool found = false;
				for(int i = 0; i < arp_entries; ++i)
					if(arp_table[i].ip == ip)
					{
						found = true;
						break;
					}
				if(!found)
				{
					// insert new entry in ARP table
					u_char mac[6];
					memcpy(mac, eth_arp->arp_sha, 6);
					if(arp_entries == arp_table_size)
					{
						// resize ARP table
						arp_table_size *= 2;
						arp_table = realloc(arp_table, arp_table_size * sizeof(struct arp_entry));
					}
					arp_table[arp_entries].ip = ip;
					memcpy(arp_table[arp_entries].mac, mac, 6);
					arp_entries++;
					// send packets in queue with corresponding IP address
					packet *pkt;
					struct ether_header *pkteth_hdr;
					struct iphdr *pktip_hdr;
					for(int i = 0; i < nr_packets_to_be_sent; ++i)
					{
						pkt = packets_to_be_sent[i];
						pktip_hdr = (struct iphdr *)(pkt->payload + sizeof(struct ether_header));
						if(pktip_hdr->daddr == ip)
						{
							pkteth_hdr = (struct ether_header*)(pkt->payload);
							memcpy(pkteth_hdr->ether_dhost, mac, 6);
							send_packet(pkt->interface, pkt);
							// remove packet from array and deallocate memory
							free(pkt);
							for(int j = i; j < nr_packets_to_be_sent - 1; ++j)
								packets_to_be_sent[j] = packets_to_be_sent[j + 1];
							nr_packets_to_be_sent--;
							i--;
						}
					}
					if(packets_array_size > 2 * (nr_packets_to_be_sent + 16))
					{
						// resize packets array
						packets_array_size /= 2;
						packets_to_be_sent = realloc(packets_to_be_sent, packets_array_size * sizeof(packet));
					}
					
				}
				continue;
			}
		}
		struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct ether_header));
		// verify checksum
		int s = ip_hdr->check;
		ip_hdr->check = 0;
		ip_hdr->check = checksum(ip_hdr, sizeof(struct iphdr));
		if(s != ip_hdr->check)
			continue;
		uint32_t ip = inet_addr(get_interface_ip(m.interface));
		// if router is the destination
		if(ip == ip_hdr->daddr)
		{
			// if ICMP
			if(ip_hdr->protocol == IPPROTO_ICMP)
			{
				struct icmphdr *icmp_hdr = (struct icmphdr*)(m.payload + sizeof(struct ether_header) + sizeof(struct iphdr));
				// if echo request, send echo reply
				if(icmp_hdr->type == ICMP_ECHO)
				{
					// icmphdr
					icmp_hdr->code = 0;
					icmp_hdr->type = 0;
					icmp_hdr->checksum = 0;
					icmp_hdr->checksum = checksum(icmp_hdr, sizeof(struct icmphdr));
					// iphdr
					__u32 addr = ip_hdr->saddr;
					ip_hdr->saddr = ip_hdr->daddr;
					ip_hdr->daddr = addr;
					ip_hdr->check = 0;
					ip_hdr->check = checksum(ip_hdr, sizeof(struct iphdr));
					//ether_header
					u_char host[6];
					memcpy(host, eth_hdr->ether_shost, 6);
					memcpy(eth_hdr->ether_shost, eth_hdr->ether_dhost, 6);
					memcpy(eth_hdr->ether_dhost, host, 6);
					send_packet(m.interface, &m);
				}
			}
			continue;
		}
		// if TTL expired in transit, send corresponding ICMP packet
		if(ip_hdr->ttl <= 1)
		{
			packet pkt;
			// icmphdr
			struct icmphdr *pkticmp_hdr = (struct icmphdr*)(pkt.payload + sizeof(struct ether_header) + sizeof(struct iphdr));
			pkticmp_hdr->code = 0;
			pkticmp_hdr->type = 11;
			pkticmp_hdr->checksum = 0;
			pkticmp_hdr->checksum = checksum(pkticmp_hdr, sizeof(struct icmphdr));
			// iphdr
			struct iphdr *pktip_hdr = (struct iphdr*)(pkt.payload + sizeof(struct ether_header));
			pktip_hdr->version = 4;
			pktip_hdr->ihl = 5;
			pktip_hdr->tos = 0;
			pktip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
			pktip_hdr->frag_off = 0;
			pktip_hdr->ttl = 64;
			pktip_hdr->protocol = IPPROTO_ICMP;
			pktip_hdr->saddr = ip;
			pktip_hdr->daddr = ip_hdr->saddr;
			pktip_hdr->check = 0;
			pktip_hdr->check = checksum(pktip_hdr, sizeof(struct iphdr));
			// ether_header
			struct ether_header *pkteth_hdr = (struct ether_header*)(pkt.payload);
			get_interface_mac(m.interface, pkteth_hdr->ether_shost);
			memcpy(pkteth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
			pkteth_hdr->ether_type = htons(ETHERTYPE_IP);
			pkt.interface = m.interface;
			pkt.len = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr);
			send_packet(m.interface, &pkt);
			continue;
		}
		// search route table for the best matching route
		struct route_table_entry *entry = NULL;
		int left = 0, right = rtable_entries - 1, mid;
		while(left <= right)
		{
			mid = left + (right - left) / 2;
			if((rtable[mid].prefix & rtable[mid].mask) == (ip_hdr->daddr & rtable[mid].mask))
			{
				if(entry == NULL || ntohl(rtable[mid].mask) > ntohl(entry->mask))
				{
					entry = &rtable[mid];
				}
				left = mid + 1;
			}
			else if(ntohl(rtable[mid].prefix & rtable[mid].mask) < ntohl(ip_hdr->daddr & rtable[mid].mask))
				left = mid + 1;
			else right = mid - 1;
		}
		// if no route was found, send corresponding ICMP packet
    	if(entry == NULL)
		{
			packet pkt;
			// icmphdr
			struct icmphdr *pkticmp_hdr = (struct icmphdr*)(pkt.payload + sizeof(struct ether_header) + sizeof(struct iphdr));
			pkticmp_hdr->code = 0;
			pkticmp_hdr->type = 3;
			pkticmp_hdr->checksum = 0;
			pkticmp_hdr->checksum = checksum(pkticmp_hdr, sizeof(struct icmphdr));
			// iphdr
			struct iphdr *pktip_hdr = (struct iphdr*)(pkt.payload + sizeof(struct ether_header));
			pktip_hdr->version = 4;
			pktip_hdr->ihl = 5;
			pktip_hdr->tos = 0;
			pktip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
			pktip_hdr->frag_off = 0;
			pktip_hdr->ttl = 64;
			pktip_hdr->protocol = IPPROTO_ICMP;
			pktip_hdr->saddr = ip;
			pktip_hdr->daddr = ip_hdr->saddr;
			pktip_hdr->check = 0;
			pktip_hdr->check = checksum(pktip_hdr, sizeof(struct iphdr));
			// ether_header
			struct ether_header *pkteth_hdr = (struct ether_header*)(pkt.payload);
			get_interface_mac(m.interface, pkteth_hdr->ether_shost);
			memcpy(pkteth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
			pkteth_hdr->ether_type = htons(ETHERTYPE_IP);
			pkt.interface = m.interface;
			pkt.len = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr);
			send_packet(m.interface, &pkt);
			continue;
		}
		// if a route was found
		// decrement TTL
		ip_hdr->ttl--;
		// update checksum
		ip_hdr->check = 0;
        ip_hdr->check = checksum(ip_hdr, sizeof(struct iphdr));
		get_interface_mac(entry->interface, eth_hdr->ether_shost);
		_Bool found = false;
		// search ARP table
		for(int i = 0; i < arp_table_size; ++i)
			if(arp_table[i].ip == entry->next_hop)
			{
				memcpy(eth_hdr->ether_dhost, arp_table[i].mac, 6);
				found = true;
				break;
			}
		// if ARP entry found, send packet
		if(found)
		{
			m.interface = entry->interface;
			send_packet(m.interface, &m);
			continue;
		}
		// if ARP entry not found
		if(packets_array_size == nr_packets_to_be_sent)
		{
			// resize packets array
			packets_array_size *= 2;
			packets_to_be_sent = realloc(packets_to_be_sent, packets_array_size * sizeof(packet));
		}
		// allocate memory for new packet
		packet *pkt = malloc(sizeof(packet));
		memcpy(pkt, &m, sizeof(packet));
		pkt->interface = entry->interface;
		// insert packet in packets array to send it later
		packets_to_be_sent[nr_packets_to_be_sent] = pkt;
		nr_packets_to_be_sent++;
		// send ARP request
		pkt = malloc(sizeof(packet));
		// ether_arp
		struct ether_arp *eth_arp = (struct ether_arp*)(pkt->payload + sizeof(struct ether_header));
		hwaddr_aton("ff:ff:ff:ff:ff:ff", eth_arp->arp_tha);
		get_interface_mac(entry->interface, eth_arp->arp_sha);
		memcpy(eth_arp->arp_tpa, &entry->next_hop, 4);
		ip = inet_addr(get_interface_ip(entry->interface));
		memcpy(eth_arp->arp_spa, &ip, 4);
		eth_arp->arp_op = htons(ARPOP_REQUEST);
		eth_arp->arp_hrd = htons(ARPHRD_ETHER);
		eth_arp->arp_pro = htons(ETHERTYPE_IP);
		eth_arp->arp_hln = 6;
		eth_arp->arp_pln = 4;
		// ether_header
		struct ether_header *pkteth_hdr = (struct ether_header*)(pkt->payload);
		get_interface_mac(entry->interface, pkteth_hdr->ether_shost);
		hwaddr_aton("ff:ff:ff:ff:ff:ff", pkteth_hdr->ether_dhost);
		pkteth_hdr->ether_type = htons(ETHERTYPE_ARP);
		pkt->interface = entry->interface;
		pkt->len = sizeof(struct ether_header) + sizeof(struct ether_arp);
		send_packet(pkt->interface, pkt);
		free(pkt);
		continue;
	}
	// free memory
	free(rtable);
	free(arp_table);
	for(int i = 0; i < nr_packets_to_be_sent; ++i)
		free(packets_to_be_sent[i]);
	free(packets_to_be_sent);
}
