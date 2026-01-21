/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 17th of January 2026, Saturday
 * @date updated 18th of January 2026, Sunday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neodriver.h"



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
const int POOL_SIZE = 1;



//===============================================================================|
//          DEFINITON
//===============================================================================|
NeoDriver::NeoDriver(const std::string& urls, BoltValue auth, BoltValue extras)
	: urls(urls), auth(auth), pool(POOL_SIZE, this->urls, &this->auth, &this->extras), 
	  pool_size(POOL_SIZE)
{
	this->extras = BoltValue::Make_Map();

	// make sure every key in the extra map is in lowercase letters
	size_t items = extras.map_val.key_offset + extras.map_val.size;
	for (size_t v = extras.map_val.size, k = extras.map_val.key_offset; k < items; k++, v++)
	{
		BoltValue* bv = GetBoltPool<BoltValue>()->Get(v);
		std::string key = Utils::String_ToLower(GetBoltPool<BoltValue>()->Get(k)->ToString());
		this->extras.Insert_Map(key, *bv);
	} // end for copy
} // end constructor


/**
 * @brief destructor
 */
NeoDriver::~NeoDriver() {}



void NeoDriver::Set_Pool_Size(const int nsize)
{
	pool_size = nsize;
} // end Set_Pool_Size


int NeoDriver::Get_Pool_Size() const
{
	return pool_size;
} // end Get_Pool_Size