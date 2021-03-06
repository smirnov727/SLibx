/*
 *  Copyright (c) 2008-2017 SLIBIO. All Rights Reserved.
 *
 *  This file is part of the SLib.io project.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CHECKHEADER_SLIB_SNET_DBIP
#define CHECKHEADER_SLIB_SNET_DBIP

/*************************************************
	DB-IP

Get Country code from ip address using the database updated from https://db-ip.com/db/download/country

For example,

   211.106.66.5 => KR

*************************************************/

#include "definition.h"

#include <slib/network/ip_address.h>

namespace slib
{

	class DbIp : public Object
	{
	public:
		const char* getCountryCode(const IPv4Address& ipv4);
		
		const char* getCountryCode(const IPv6Address& ipv6);

	public:
		static Ref<DbIp> create(const void* dbCSV, sl_size size);
		
		static Ref<DbIp> create(const String& pathToCSVFile);

	public:
		struct IPv4Item
		{
			sl_uint32 start;
			sl_uint32 end;
			char code[4];
		};
	private:
		List<IPv4Item> m_listIPv4;
		IPv4Item* m_ipv4;
		sl_uint32 m_countIPv4;

	public:
		struct IPv6Item
		{
			IPv6Address start;
			IPv6Address end;
			char code[4];
		};
	private:
		List<IPv6Item> m_listIPv6;
		IPv6Item* m_ipv6;
		sl_uint32 m_countIPv6;
		
	};

}

#endif

