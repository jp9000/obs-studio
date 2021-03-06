/******************************************************************************
    Copyright (C) 2016 B. Lee <bl4@postpile.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "net-if.h"
#include <util/platform.h>
#include <util/dstr.h>

#define do_log(level, format, ...) \
	blog(level, "[net if] " format, ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

static inline void netif_saddr_data_push_back(struct netif_saddr_data *sd,
					      const char *ip,
					      const char *adapter)
{
	struct netif_saddr_item item;
	struct dstr full_name = {0};
	char *ip_dup = bstrdup(ip);

	if (adapter && *adapter)
		dstr_printf(&full_name, "[%s] %s", adapter, ip);
	else
		dstr_copy(&full_name, ip);

	item.name = full_name.array;
	item.addr = ip_dup;

	da_push_back(sd->addrs, &item);
}

static void netif_convert_to_string(char *dest,
				    struct sockaddr_storage *byte_address)
{
	int family = byte_address->ss_family;
	char temp_char[INET6_ADDRSTRLEN] = {0};

#ifndef _WIN32
	if (family == AF_INET)
		inet_ntop(family,
			  &(((struct sockaddr_in *)byte_address)->sin_addr),
			  temp_char, INET6_ADDRSTRLEN);
	else if (family == AF_INET6)
		inet_ntop(family,
			  &(((struct sockaddr_in *)byte_address)->sin_addr),
			  temp_char, INET6_ADDRSTRLEN);
#else
	if (family == AF_INET)
		InetNtopA(family, &(((SOCKADDR_IN *)byte_address)->sin_addr),
			  temp_char, INET6_ADDRSTRLEN);
	else if (family == AF_INET6)
		InetNtopA(family, &(((SOCKADDR_IN6 *)byte_address)->sin6_addr),
			  temp_char, INET6_ADDRSTRLEN);
#endif
	strncpy(dest, temp_char, INET6_ADDRSTRLEN);
}

static void netif_push(struct sockaddr *copy_source,
		       struct netif_saddr_data *saddr_d, const char *adapter)
{
	char temp_char[INET6_ADDRSTRLEN] = {0};
	struct sockaddr_storage sa = {0};

	if (copy_source->sa_family == AF_INET)
		memcpy(&sa, copy_source, sizeof(struct sockaddr_in));
	else if (copy_source->sa_family == AF_INET6)
		memcpy(&sa, copy_source, sizeof(struct sockaddr_in6));

	netif_convert_to_string(temp_char, &sa);
	netif_saddr_data_push_back(saddr_d, temp_char, adapter);
}

void netif_log_saddrs(struct netif_saddr_data *sd)
{
	for (size_t i = 0; i < sd->addrs.num; i++)
		info("\t\t%s", sd->addrs.array[i].name);
}

bool netif_str_to_addr(struct sockaddr_storage *out, int *addr_len,
		       const char *addr)
{
	bool ipv6;

	memset(out, 0, sizeof(*out));
	*addr_len = 0;

	if (!addr)
		return false;

	ipv6 = (strchr(addr, ':') != NULL);
	out->ss_family = ipv6 ? AF_INET6 : AF_INET;
	*addr_len = sizeof(*out);

#ifdef _WIN32
	int ret = WSAStringToAddressA((LPSTR)addr, out->ss_family, NULL,
				      (LPSOCKADDR)out, addr_len);
	if (ret == SOCKET_ERROR)
		warn("Could not parse address, error code: %d", GetLastError());
	return ret != SOCKET_ERROR;
#else
	struct sockaddr_in *sin = (struct sockaddr_in *)out;
	if (inet_pton(out->ss_family, addr, &sin->sin_addr)) {
		*addr_len = ipv6 ? sizeof(struct sockaddr_in6)
				 : sizeof(struct sockaddr_in);
		return true;
	}

	return false;
#endif
}

#ifndef _WIN32
static inline bool is_loopback(struct ifaddrs *ifa)
{
	const char *n = ifa->ifa_name;
	return n && (strcmp(n, "lo") == 0 || strcmp(n, "lo0") == 0);
}

static inline void netif_get_addrs_nix(struct netif_saddr_data *ifaddrs)
{
	struct ifaddrs *ifaddr, *ifa;
	unsigned int family, s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		warn("getifaddrs() failed");
		return;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL || is_loopback(ifa))
			continue;

		family = ifa->ifa_addr->sa_family;

		if ((family == AF_INET) || (family == AF_INET6)) {
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET)
						? sizeof(struct sockaddr_in)
						: sizeof(struct sockaddr_in6),
					host, NI_MAXHOST, NULL, 0,
					NI_NUMERICHOST);
			if (s != 0) {
				warn("getnameinfo() failed: %s",
				     gai_strerror(s));
				continue;
			}

			netif_push(ifa->ifa_addr, ifaddrs, ifa->ifa_name);
		}
	}

	freeifaddrs(ifaddr);
}

#else

static inline PIP_ADAPTER_ADDRESSES get_adapters(void)
{
	PIP_ADAPTER_ADDRESSES adapter = NULL;
	unsigned long ret = 0;
	unsigned long out_buf_len = 16384;
	unsigned long flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
			      GAA_FLAG_SKIP_DNS_SERVER;
	const int max_tries = 3;
	int i = 0;

	do {
		adapter = (IP_ADAPTER_ADDRESSES *)bmalloc(out_buf_len);
		if (!adapter)
			return NULL;

		ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapter,
					   &out_buf_len);
		if (ret == ERROR_BUFFER_OVERFLOW) {
			bfree(adapter);
			adapter = NULL;
		} else {
			break;
		}
		i++;
	} while ((ret == ERROR_BUFFER_OVERFLOW) && (i < max_tries));

	if (ret != NO_ERROR && ret != ERROR_NO_DATA) {
		LPSTR msg_buf = NULL;

		bfree(adapter);
		adapter = NULL;

		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				       FORMAT_MESSAGE_FROM_SYSTEM |
				       FORMAT_MESSAGE_IGNORE_INSERTS,
			       NULL, ret,
			       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			       (LPSTR)&msg_buf, 0, NULL);
		if (msg_buf) {
			warn("Call to GetAdaptersAddresses failed: %s (%d)",
			     msg_buf, ret);
			LocalFree(msg_buf);
		}
	}

	return adapter;
}

static inline void netif_get_addrs_win32(struct netif_saddr_data *ifaddrs)
{
	PIP_ADAPTER_ADDRESSES adapter = get_adapters();
	PIP_ADAPTER_UNICAST_ADDRESS unicast = NULL;
	PIP_ADAPTER_ADDRESSES cur_adap = NULL;
	SOCKET_ADDRESS socket_addr;
	int family;

	if (!adapter)
		return;

	for (cur_adap = adapter; !!cur_adap; cur_adap = cur_adap->Next) {
		char *adap_name = NULL;

		if (cur_adap->OperStatus != IfOperStatusUp ||
		    cur_adap->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
			continue;

		os_wcs_to_utf8_ptr(cur_adap->FriendlyName, 0, &adap_name);

		unicast = cur_adap->FirstUnicastAddress;

		for (; !!unicast; unicast = unicast->Next) {
			socket_addr = unicast->Address;

			family = socket_addr.lpSockaddr->sa_family;
			if (family == AF_INET || family == AF_INET6)
				netif_push(socket_addr.lpSockaddr, ifaddrs,
					   adap_name);
		}

		bfree(adap_name);
	}

	bfree(adapter);
}
#endif

void netif_get_addrs(struct netif_saddr_data *ifaddrs)
{
	da_init(ifaddrs->addrs);

#ifdef _WIN32
	netif_get_addrs_win32(ifaddrs);
#else
	netif_get_addrs_nix(ifaddrs);
#endif
}
